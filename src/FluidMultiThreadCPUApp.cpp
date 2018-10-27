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
#include <vector>

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

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci;
using namespace ci::app;
using namespace std;

typedef struct{
    float surfaceThreshold;
    float contentThreshold;
} ThresholdConfig;

typedef struct{
    float circleRadius;
} GeometryConfig;

class FluidMultiThreadCPUApp : public App {
private:
    Simulator s;
    u_long n;
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
    
    gl::FboRef mMainFBO, mTempFBO;
    
    ThresholdConfig thresholdConfig;
    GeometryConfig geometryConfig;
    
    // Draw the base shape of fluid particle
    void drawParticle();
    // Apply blur shader to the particle
    void blurParticle();
    // Apply horizontal only blur
    void horizontalBlur();
    // Apply vertical only blur
    void verticalBlur();
    // Apply thresholding
    void thresholdParticle();
    
public:
    void setup() override;
    void mouseDown( MouseEvent event ) override;
    void update() override;
    void draw() override;
    void prepareSettings(Settings *settings);
};

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
        
        blurShader = gl::GlslProg::create( gl::GlslProg::Format()
                                           .vertex    ( loadResource("blur.vert" ) )
                                           .fragment  ( loadResource( "blur.frag" ) ) );
        thresholdShader = gl::GlslProg::create( gl::GlslProg::Format()
                                                .vertex    ( loadResource("blur.vert" ) )
                                                .fragment  ( loadResource( "threshold.frag" ) ) );
        thresholdConfig.surfaceThreshold = 0.1f;
        thresholdConfig.contentThreshold = 0.2f;

    }
    catch( gl::GlslProgCompileExc ex ) {
        CI_LOG_E( ex.what() );
        quit();
    }
    
    gl::Batch::AttributeMapping mapping( { { geom::Attrib::CUSTOM_9, "trailPosition" } } );
    mParticleBatch = gl::Batch::create(mesh, baseParticleShader, mapping);
    gl::pointSize(4.0f);
    
    mMainFBO = gl::Fbo::create( 1600, 800, gl::Fbo::Format().colorTexture()  );
    mTempFBO = gl::Fbo::create( 1600, 800, gl::Fbo::Format().colorTexture()  );

#else
    mParticleBatch = gl::Batch::create( mesh, gl::GlslProg::create( loadAsset( "draw_es3.vert" ), loadAsset( "draw_es3.frag" ) ),mapping );
#endif
    
}

void FluidMultiThreadCPUApp::mouseDown( MouseEvent event )
{
}

void FluidMultiThreadCPUApp::update()
{
    s.update();
    
    // Copy particle data onto the GPU.
    // Map the GPU memory and write over it.
    void *gpuMem = mParticleVbo->mapReplace();
    memcpy( gpuMem, s.particles.data(), s.particles.size() * sizeof(Particle) );
    mParticleVbo->unmap();
}

void FluidMultiThreadCPUApp::drawParticle() {
    gl::ScopedFramebuffer fbo(mMainFBO);
    baseParticleShader->uniform("circleRadius", geometryConfig.circleRadius);
    
    gl::clear( Color::black() );
    mParticleBatch->draw();
}

void FluidMultiThreadCPUApp::blurParticle() {
    gl::ScopedGlslProg shader( blurShader );
    blurShader->uniform( "tex0", 0 ); // use texture unit 0
    
    // tell the shader to blur horizontally and the size of 1 pixel
    blurShader->uniform( "sample_offset", vec2( 1.0f / mTempFBO->getWidth(), 0.0f ) );
    horizontalBlur();
    blurShader->uniform( "sample_offset", vec2( 0.0f, 1.0f / mMainFBO->getHeight() ) );
    verticalBlur();
}

void FluidMultiThreadCPUApp::horizontalBlur() {
    defer(swap(mMainFBO, mTempFBO));
    gl::ScopedFramebuffer fbo( mTempFBO );
    gl::ScopedTextureBind tex0( mMainFBO->getColorTexture(), (uint8_t)0 );
    
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    
    gl::drawSolidRect( mTempFBO->getBounds() );
}

void FluidMultiThreadCPUApp::verticalBlur() {
    defer(swap(mMainFBO, mTempFBO));
    gl::ScopedFramebuffer fbo( mTempFBO );
    gl::ScopedTextureBind tex0( mMainFBO->getColorTexture(), (uint8_t)0 );
    
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    
    gl::drawSolidRect( mTempFBO->getBounds() );
}

void FluidMultiThreadCPUApp::thresholdParticle() {
    gl::ScopedGlslProg shader( thresholdShader );
    thresholdShader->uniform( "tex0", 0 );
    thresholdShader->uniform( "surfaceThreshold", thresholdConfig.surfaceThreshold );
    thresholdShader->uniform( "contentThreshold", thresholdConfig.contentThreshold );
    
    defer(swap(mMainFBO, mTempFBO));
    gl::ScopedFramebuffer fbo( mTempFBO );
    gl::ScopedTextureBind tex0( mMainFBO->getColorTexture(), (uint8_t)0 );
    
    gl::setMatricesWindowPersp( getWindowSize());
    gl::clear( Color::black() );
    
    gl::drawSolidRect( mTempFBO->getBounds() );

}

void FluidMultiThreadCPUApp::draw()
{
    drawParticle();
    blurParticle();
    thresholdParticle();
    
    gl::clear( Color::black() );
    ImGui::SliderFloat( "Surface Threshold", &thresholdConfig.surfaceThreshold, 0, 1 );
    ImGui::SliderFloat( "Content Threshold", &thresholdConfig.contentThreshold, 0, 1 );
    ImGui::SliderFloat( "Circle Radius", &geometryConfig.circleRadius, 1, 30 );
    gl::draw(mMainFBO->getColorTexture());
    
}


CINDER_APP( FluidMultiThreadCPUApp, RendererGl , [] ( App::Settings *settings ) {
    settings->setWindowSize( 1600, 800 );
    settings->setMultiTouchEnabled( false );
} )
