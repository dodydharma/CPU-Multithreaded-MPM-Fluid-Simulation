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

#include "CinderImGui.h"

#include "Simulator.cpp"
#include <vector>

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci;
using namespace ci::app;
using namespace std;

class FluidMultiThreadCPUApp : public App {
    Simulator s;
    int n;
    GLfloat*            vertices;
    ColorA*             colors;
    
    // Buffer holding raw particle data on GPU, written to every update().
    gl::VboRef			mParticleVbo;
    // Batch for rendering particles with  shader.
    gl::BatchRef		mParticleBatch;
    //    gl::BatchRef		mRect;
    gl::GlslProgRef     mGlsl;
    gl::GlslProgRef     mShaderBlur;
    
    gl::TextureRef		mTexture;
    
    gl::FboRef mFBO, mFboBlur1, mFboBlur2;
    
public:
    void setup() override;
    void mouseDown( MouseEvent event ) override;
    void update() override;
    void draw() override;
    void prepareSettings(Settings *settings);
};

void FluidMultiThreadCPUApp::setup()
{
    ImGui::initialize();
    
    s.initializeGrid(400,200);
    s.addParticles();
    s.scale = 4.0f;
    n = s.particles.size();
    printf("jumlah partikel %d",n);
    
    mParticleVbo = gl::Vbo::create( GL_ARRAY_BUFFER, s.particles, GL_STREAM_DRAW );
    // Describe particle semantics for GPU.
    geom::BufferLayout particleLayout;
    particleLayout.append( geom::Attrib::POSITION, 3, sizeof( Particle ), offsetof( Particle, pos ) );
    particleLayout.append( geom::Attrib::CUSTOM_9, 3, sizeof( Particle ), offsetof( Particle, trail ) );
    particleLayout.append( geom::Attrib::COLOR, 4, sizeof( Particle ), offsetof( Particle, color ) );
    
    // Create mesh by pairing our particle layout with our particle Vbo.
    // A VboMesh is an array of layout + vbo pairs
    auto mesh = gl::VboMesh::create( s.particles.size(), GL_POINTS, { { particleLayout, mParticleVbo } } );
    
#if ! defined( CINDER_GL_ES )
    // setup shader
    try {
        mGlsl = gl::GlslProg::create( gl::GlslProg::Format()
                                     .vertex    ( loadResource("vertex.vert" ) )
                                     .geometry  ( loadResource( "geometry.geom" ) )
                                     .fragment  ( loadResource( "fragment.frag" ) ) );
        mShaderBlur = gl::GlslProg::create( gl::GlslProg::Format()
                                           .vertex    ( loadResource("blur.vert" ) )
                                           .fragment  ( loadResource( "blur.frag" ) ) );
    }
    catch( gl::GlslProgCompileExc ex ) {
        cout << ex.what() << endl;
        quit();
    }
    
    gl::Batch::AttributeMapping mapping( { { geom::Attrib::CUSTOM_9, "trailPosition" } } );
    mParticleBatch = gl::Batch::create(mesh, mGlsl, mapping);
    gl::pointSize(4.0f);
    
    mFBO = gl::Fbo::create( 1600, 800, gl::Fbo::Format().colorTexture()  );
    mFboBlur1 = gl::Fbo::create( 1600, 800, gl::Fbo::Format().colorTexture()  );
    gl::Fbo::Format csaaFormat;
    //    csaaFormat.setSamples( 4 );
    csaaFormat.colorTexture();
    mFboBlur2 = gl::Fbo::create( 1600, 800, csaaFormat  );
    
    //    mRect = gl::Batch::create( geom::Plane(), mGlsl2 );
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

void FluidMultiThreadCPUApp::draw()
{
    mFBO->bindFramebuffer();
    // clear out the window with black
    gl::clear( Color::black() );
    gl::setMatricesWindowPersp( getWindowSize());
    gl::enableDepthRead();
    gl::enableDepthWrite();
    //    gl::enableAlphaBlending();
    //    gl::enableAdditiveBlending();
    //    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    //    gl::enableAlphaBlendingPremult();
    
    mParticleBatch->draw();
    
    mFBO->unbindFramebuffer();
    
    {
        gl::ScopedGlslProg shader( mShaderBlur );
        mShaderBlur->uniform( "tex0", 0 ); // use texture unit 0
        
        // tell the shader to blur horizontally and the size of 1 pixel
        mShaderBlur->uniform( "sample_offset", vec2( 1.0f / mFboBlur1->getWidth(), 0.0f ) );
        mShaderBlur->uniform( "attenuation", 1.0f );
        mShaderBlur->uniform( "use_threshold", false );
        
        // copy a horizontally blurred version of our scene into the first blur Fbo
        {
            gl::ScopedFramebuffer fbo( mFboBlur1 );
            gl::ScopedViewport    viewport( 0, 0, mFboBlur1->getWidth(), mFboBlur1->getHeight() );
            
            gl::ScopedTextureBind tex0( mFBO->getColorTexture(), (uint8_t)0 );
            
            
            gl::setMatricesWindowPersp( getWindowSize());
            gl::clear( Color::black() );
            
            gl::drawSolidRect( mFboBlur1->getBounds() );
        }
        
        // tell the shader to blur vertically and the size of 1 pixel
        mShaderBlur->uniform( "sample_offset", vec2( 0.0f, 1.0f / mFboBlur2->getHeight() ) );
        mShaderBlur->uniform( "attenuation", 1.0f );
        mShaderBlur->uniform( "use_threshold", true );
        
        // copy a vertically blurred version of our blurred scene into the second blur Fbo
        {
            gl::ScopedFramebuffer fbo( mFboBlur2 );
            gl::ScopedViewport    viewport( 0, 0, mFboBlur2->getWidth(), mFboBlur2->getHeight() );
            
            gl::ScopedTextureBind tex0( mFboBlur1->getColorTexture(), (uint8_t)0 );
            
            
            gl::setMatricesWindowPersp( getWindowSize());
            gl::clear( Color::black() );
            
            gl::drawSolidRect( mFboBlur2->getBounds() );
        }
    }
    
    gl::clear( Color::black() );
    gl::setMatricesWindowPersp( getWindowSize());
    gl::enableDepthRead();
    gl::enableDepthWrite();
    //    mGlsl2->bind();
    gl::clear( Color( 0, 0, 0 ) );
    float minRadius = 0;
    ImGui::SliderFloat( "Min Radius", &minRadius, 1, 499 );
//    gl::clear( Color( 1, 1, 1 ) );
    gl::draw(mFboBlur2->getColorTexture());
    
    
}


CINDER_APP( FluidMultiThreadCPUApp, RendererGl , [] ( App::Settings *settings ) {
    settings->setWindowSize( 1600, 800 );
    settings->setMultiTouchEnabled( false );
} )
