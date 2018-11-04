//
//  Rendering with Vertex Buffer Object, by Dody Dharma on May 2016
//

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/ImageIo.h"
#include "cinder/Log.h"

#include "CinderImGui.h"

#include "Simulator.cpp"
#include "GaussianKernel.hpp"
#include <vector>
#include <cstdio>
#include <functional>

// Begin c++ defer function hack
template <typename F>
struct saucy_defer {
    F f;
    saucy_defer(F f) : f(f) {}
    ~saucy_defer() { f(); }
};

template <typename F>
saucy_defer<F> defer_func(F f) {
    return saucy_defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) =     defer_func([&](){code;})
// End c++ defer function hack

#define UV_PATTERN_NUM 2

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci;
using namespace ci::app;
using namespace std;

typedef struct{
    float surfaceThreshold = 0.1f; // Base particle brightness required to be surface pixel(0..1)
    float contentThreshold = 0.2f; // Base particle brightness required to be content pixel(0..1)
    float uvThreshold = 0.147f; // UV Map brightness required to be UV pattern pixel(0..1)
    int uvPattern = 0;
} ThresholdConfig;

typedef struct{
    float circleRadius = 7.0f;
    int triangleCount = 6;
    float uvRedGreenParity = 0.5f; // Determines isUVRed chance to be green(0..1)
} GeometryConfig;

typedef struct {
    bool drawBaseParticle = true;
    bool drawUVParticle = true;
    bool blurBaseParticle = true;
    bool blurUVParticle = true;
    bool thresholdBaseParticle = true;
    int framebufferType = 0; // 0 means showing the final base particle result, 1 means showing the final uv particle result
} RenderPipelineConfig;

class FluidMultiThreadCPUApp : public App {
private:
    Simulator s;
    u_long n;
    bool isSimulationPaused = false;
    GLfloat*            vertices;
    ColorA*             colors;

    // Buffer holding raw particle data on GPU, written to every update().
    gl::VboRef			mParticleVbo;
    // Batch for rendering particles with  shader.
    gl::BatchRef		mParticleBatch;
    //    gl::BatchRef		mRect;
    gl::GlslProgRef     baseParticleShader;
    gl::GlslProgRef     blurShader;
    gl::GlslProgRef     thresholdShader;
    
    gl::TextureRef		mTexture;
    
    gl::FboRef mMainFBO, mTempFBO, mUVFBO;
    
    ThresholdConfig thresholdConfig;
    GeometryConfig geometryConfig;
    GaussianKernelConfig gaussianKernelConfig;
    GaussianKernel gaussianKernel;
    RenderPipelineConfig renderPipelineConfig;
    
    // Draw the base shape of fluid particle
    void drawParticle();
    // Draw the base shape of UV particle
    void drawUVParticle();
    // Apply horizontal-vertical blur shader to target FboRef
    void blurParticle(gl::FboRef&);
    // Apply horizontal only blur
    void horizontalBlur(gl::FboRef&);
    // Apply vertical only blur
    void verticalBlur(gl::FboRef&);
    // Apply thresholding
    void thresholdParticle();
    // Draw the Gaussian 2D kernel
    void drawGaussianBlurGrid();
    void drawParamterUI();
    void drawRenderConfigUI();
    
public:
    void setup() override;
    void mouseDown( MouseEvent event ) override;
    void update() override;
    void draw() override;
    void prepareSettings(Settings *settings);
};

vector<float> gaussianFunction(float, int, int);

void FluidMultiThreadCPUApp::setup()
{
    gl::setMatricesWindowPersp( getWindowSize());
    gl::enableDepthRead();
    gl::enableDepthWrite();
    
    ImGui::initialize();
    
    s.initializeGrid(400,200);
    s.addParticles();
    s.scale = 4.0f;
    n = s.particles.size();
    
    mParticleVbo = gl::Vbo::create( GL_ARRAY_BUFFER, s.particles, GL_STREAM_DRAW );
    // Describe particle semantics for GPU.
    geom::BufferLayout particleLayout;
    particleLayout.append( geom::Attrib::POSITION, 3, sizeof( Particle ), offsetof( Particle, pos ) );
    particleLayout.append( geom::Attrib::CUSTOM_9, 3, sizeof( Particle ), offsetof( Particle, trail ) );
    particleLayout.append( geom::Attrib::CUSTOM_8, 1, sizeof( Particle ), offsetof( Particle, isUVRed ) );
    particleLayout.append( geom::Attrib::COLOR, 4, sizeof( Particle ), offsetof( Particle, color ) );
    
    // Create mesh by pairing our particle layout with our particle Vbo.
    // A VboMesh is an array of layout + vbo pairs
    auto mesh = gl::VboMesh::create( (u_int)s.particles.size(), GL_POINTS, { { particleLayout, mParticleVbo } } );
    
#if ! defined( CINDER_GL_ES )
    // setup shader
    try {
        baseParticleShader = gl::GlslProg::create( gl::GlslProg::Format()
                                     .vertex    ( loadResource("vertex.vert" ) )
                                     .geometry  ( loadResource( "geometry.geom" ) )
                                     .fragment  ( loadResource( "fragment.frag" ) ) );
        geometryConfig.circleRadius = 7.0f;
        geometryConfig.triangleCount = 6;
        geometryConfig.uvRedGreenParity = 0.5f;
        
        blurShader = gl::GlslProg::create( gl::GlslProg::Format()
                                           .vertex    ( loadResource("blur.vert" ) )
                                           .fragment  ( loadResource( "blur.frag" ) ) );
        
        thresholdShader = gl::GlslProg::create( gl::GlslProg::Format()
                                                .vertex    ( loadResource("blur.vert" ) )
                                                .fragment  ( loadResource( "threshold.frag" ) ) );

    }
    catch( gl::GlslProgCompileExc ex ) {
        CI_LOG_E( ex.what() );
        quit();
    }
    
    gl::Batch::AttributeMapping mapping( {
        { geom::Attrib::CUSTOM_9, "trailPosition" },
        { geom::Attrib::CUSTOM_8, "isUVRed" }
    } );
    mParticleBatch = gl::Batch::create(mesh, baseParticleShader, mapping);
    
    mMainFBO = gl::Fbo::create( 1600, 800, gl::Fbo::Format().colorTexture()  );
    mTempFBO = gl::Fbo::create( 1600, 800, gl::Fbo::Format().colorTexture()  );
    mUVFBO = gl::Fbo::create( 1600, 800, gl::Fbo::Format().colorTexture()  );

#else
    mParticleBatch = gl::Batch::create( mesh, gl::GlslProg::create( loadAsset( "draw_es3.vert" ), loadAsset( "draw_es3.frag" ) ),mapping );
#endif
    
}

void FluidMultiThreadCPUApp::mouseDown( MouseEvent event )
{
}

void FluidMultiThreadCPUApp::update()
{
    if (!isSimulationPaused) {
        s.update();
    }
    
    // Copy particle data onto the GPU.
    // Map the GPU memory and write over it.
    void *gpuMem = mParticleVbo->mapReplace();
    memcpy( gpuMem, s.particles.data(), s.particles.size() * sizeof(Particle) );
    mParticleVbo->unmap();
}

void FluidMultiThreadCPUApp::drawParticle() {
    gl::ScopedFramebuffer fbo(mMainFBO);
    baseParticleShader->uniform("isUVMap", false);
    baseParticleShader->uniform("circleRadius", geometryConfig.circleRadius);
    baseParticleShader->uniform("triangleCount", geometryConfig.triangleCount);
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    mParticleBatch->draw();
}

void FluidMultiThreadCPUApp::drawUVParticle() {
    gl::ScopedFramebuffer fbo(mUVFBO);
    baseParticleShader->uniform("isUVMap", true);
    baseParticleShader->uniform("circleRadius", geometryConfig.circleRadius);
    baseParticleShader->uniform("triangleCount", geometryConfig.triangleCount);
    baseParticleShader->uniform("uvRedGreenParity", geometryConfig.uvRedGreenParity);
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    mParticleBatch->draw();
}

void FluidMultiThreadCPUApp::blurParticle(gl::FboRef &targetFBO) {
    gl::ScopedGlslProg shader( blurShader );
    blurShader->uniform("tex0", 0 ); // use texture unit 0
    blurShader->uniform("kernelSize", gaussianKernelConfig.kernelSize);
    blurShader->uniform("kernel", &gaussianKernel.generate(gaussianKernelConfig)[0], gaussianKernelConfig.kernelSize);
    
    // tell the shader to blur horizontally and the size of 1 pixel
    blurShader->uniform( "sample_offset", vec2( 1.0f / mTempFBO->getWidth(), 0.0f ) );
    horizontalBlur(targetFBO);
    blurShader->uniform( "sample_offset", vec2( 0.0f, 1.0f / mMainFBO->getHeight() ) );
    verticalBlur(targetFBO);
}

void FluidMultiThreadCPUApp::horizontalBlur(gl::FboRef &targetFBO) {
    defer(swap(targetFBO, mTempFBO));
    gl::ScopedFramebuffer fbo( mTempFBO );
    gl::ScopedTextureBind tex0( targetFBO->getColorTexture(), (uint8_t)0 );
    
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    
    gl::drawSolidRect( mTempFBO->getBounds() );
}

void FluidMultiThreadCPUApp::verticalBlur(gl::FboRef &targetFBO) {
    defer(swap(targetFBO, mTempFBO));
    gl::ScopedFramebuffer fbo( mTempFBO );
    gl::ScopedTextureBind tex0( targetFBO->getColorTexture(), (uint8_t)0 );
    
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    
    gl::drawSolidRect( mTempFBO->getBounds() );
}

void FluidMultiThreadCPUApp::thresholdParticle() {
    gl::ScopedGlslProg shader( thresholdShader );
    thresholdShader->uniform( "tex0", 0 );
    thresholdShader->uniform( "uvTex", 1 );
    thresholdShader->uniform( "surfaceThreshold", thresholdConfig.surfaceThreshold );
    thresholdShader->uniform( "contentThreshold", thresholdConfig.contentThreshold );
    thresholdShader->uniform( "uvThreshold", thresholdConfig.uvThreshold );
    thresholdShader->uniform( "uvPattern", thresholdConfig.uvPattern );
    
    defer(swap(mMainFBO, mTempFBO));
    gl::ScopedFramebuffer fbo( mTempFBO );
    gl::ScopedTextureBind tex0( mMainFBO->getColorTexture(), (uint8_t)0 );
    gl::ScopedTextureBind tex1( mUVFBO->getColorTexture(), (uint8_t)1 );
    
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    
    gl::drawSolidRect( mTempFBO->getBounds() );

}

void FluidMultiThreadCPUApp::drawGaussianBlurGrid() {
    ImGui::Text("Generated Gaussian Blur Texture:");
    float grid_size = 300;
    float padded_cell_size = grid_size/gaussianKernelConfig.kernelSize;
    float cell_size = padded_cell_size;
    int num_columns = gaussianKernelConfig.kernelSize;
    int num_row = num_columns;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    pos.x += 10;
    pos.y += 10;
    vector<float> kernel = gaussianKernel.generate(gaussianKernelConfig);
    float max2DKernelValue = kernel[gaussianKernelConfig.kernelSize/2]*kernel[gaussianKernelConfig.kernelSize/2];
    for (int i = 0; i < num_columns; ++i)
    {
        for (int j = 0; j < num_row; ++j)
        {
            //work out the rectangle bounds for the palette cell
            float top = j * padded_cell_size;
            float left = i * padded_cell_size;
            float bottom = top + cell_size;
            float right = left + cell_size;
            
            
            uint8_t r = (uint8_t)255*kernel[i]*kernel[j]/max2DKernelValue;
            uint8_t g = r;
            uint8_t b = r;
            uint8_t a = 255;
            uint32_t rgba = (a<<24) + (r<<16) + (g<<8) + b;
            
            //draw the rectangle filled with this colour, representing a single cell
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x + left, pos.y + top), ImVec2(pos.x + right, pos.y + bottom), rgba);
            
            //set the position of the button to the cursor_pos
            ImVec2 cursor_pos = ImVec2(pos.x + left, pos.y + top);
            ImGui::SetCursorScreenPos(cursor_pos);
            
        }
        
    }
}

void FluidMultiThreadCPUApp::drawParamterUI() {
    ImGui::Begin("Parameters");
    ImGui::Text("Framerate %f", getAverageFps());
    ImGui::SliderFloat( "Surface Threshold", &thresholdConfig.surfaceThreshold, 0, 1 );
    ImGui::SliderFloat( "Content Threshold", &thresholdConfig.contentThreshold, 0, 1 );
    ImGui::SliderFloat( "UV Threshold", &thresholdConfig.uvThreshold, 0, 1 );
    ImGui::SliderFloat( "UV Red Green Parity", &geometryConfig.uvRedGreenParity, 0, 1 );
    ImGui::SliderFloat( "Circle Radius", &geometryConfig.circleRadius, 1, 30 );
    ImGui::SliderInt( "Triangle Count", &geometryConfig.triangleCount, 1, 30 );
    if (isSimulationPaused) {
        if (ImGui::Button("Play"))
        {
            isSimulationPaused = false;
        }
    } else {
        if (ImGui::Button("Pause"))
        {
            isSimulationPaused = true;
        }
    }
    for (int i = 0 ; i < UV_PATTERN_NUM; ++i) {
        char label [20];
        sprintf(label, "UV Pattern %d", i);
        if (ImGui::RadioButton(label, &thresholdConfig.uvPattern, i)) {
            thresholdConfig.uvPattern = i;
        }
    }
    ImGui::Text("Gaussian Blur Parameter:");
    ImGui::SliderFloat("Standard Deviation", &gaussianKernelConfig.sigma, 1, 10);
    ImGui::SliderInt("Kernel Size", &gaussianKernelConfig.kernelSize, 1, 25);
    if (gaussianKernelConfig.kernelSize%2 == 0) {
        gaussianKernelConfig.kernelSize++;
    }
    ImGui::SliderInt("Sample Count", &gaussianKernelConfig.sampleCount, 100, 10000);
    drawGaussianBlurGrid();
    ImGui::End();

}

void FluidMultiThreadCPUApp::drawRenderConfigUI() {
    ImGui::Begin("Render Pipeline Configuration");
    ImGui::Columns(2, NULL, true);
    ImGui::Checkbox("Draw Particle", &renderPipelineConfig.drawBaseParticle);
    ImGui::Checkbox("Blur Particle", &renderPipelineConfig.blurBaseParticle);
    ImGui::Checkbox("Threshold Particle", &renderPipelineConfig.thresholdBaseParticle);
    if (ImGui::RadioButton("Show Fluid Particle", &renderPipelineConfig.framebufferType, 0)) {
        renderPipelineConfig.framebufferType = 0;
    }
    ImGui::NextColumn();
    ImGui::Checkbox("Draw UV Particle", &renderPipelineConfig.drawUVParticle);
    ImGui::Checkbox("Blur UV Particle", &renderPipelineConfig.blurUVParticle);
    if (ImGui::RadioButton("Show UV Particle", &renderPipelineConfig.framebufferType, 1)) {
        renderPipelineConfig.framebufferType = 1;
    }
    ImGui::Columns(1);
    ImGui::End();
}

void FluidMultiThreadCPUApp::draw()
{
    if (renderPipelineConfig.drawBaseParticle) {
        drawParticle();
    }
    if (renderPipelineConfig.drawUVParticle) {
        drawUVParticle();
    }
    if (renderPipelineConfig.blurBaseParticle) {
        blurParticle(mMainFBO);
    }
    if (renderPipelineConfig.blurUVParticle) {
        blurParticle(mUVFBO);
    }
    if (renderPipelineConfig.thresholdBaseParticle) {
        thresholdParticle();
    }
    
    gl::clear( Color::black() );
    drawParamterUI();
    drawRenderConfigUI();
    
    switch (renderPipelineConfig.framebufferType) {
        case 0:
            gl::draw(mMainFBO->getColorTexture());
            break;
        case 1:
            gl::draw(mUVFBO->getColorTexture());
        default:
            CI_LOG_E("error framebuffer type");
            break;
    }
    
    
}

CINDER_APP( FluidMultiThreadCPUApp, RendererGl , [] ( App::Settings *settings ) {
    settings->setWindowSize( 1600, 800 );
    settings->setMultiTouchEnabled( false );
} )
