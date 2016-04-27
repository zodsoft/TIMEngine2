#include "MainRenderer.h"
#include "Rand.h"
#include <QThread>

#include "MemoryLoggerOn.h"
namespace tim{

#undef interface
using namespace core;
using namespace renderer;
using namespace resource;
using namespace interface;

MainRenderer::MainRenderer(RendererWidget* parent) : _parent(parent)
{
    _renderingParameter.useShadow = false;
    _renderingParameter.usePointLight = false;
    _currentSize = {200,200};

    _running = true;

    _view[0].camera.ratio = 1;
    _view[0].camera.clipDist = {.1,1000};
    _view[0].camera.pos = {3,3,3};
    _view[0].camera.dir = {0,0,0};
}

void MainRenderer::main()
{
    lock();

    ShaderPool::instance().add("gPass", "shader/gBufferPass.vert", "shader/gBufferPass.frag").value();

    _pipeline.setScene(_scene[0], _view[0]);

    unlock();
    while(_running)
    {
        lock();

        _eventMutex.lock();
        while(!_events.empty())
            _events.pop()();
        _eventMutex.unlock();

        if(_pipeline.pipeline)
        {
            openGL.clearColor({1,0,0,0});
            _pipeline.pipeline->prepare();
            _pipeline.pipeline->render();
        }

        resize();
        _pipeline.setScene(_scene[0], _view[0]);
        unlock();

        GL_ASSERT();
        _parent->swapBuffers();
    }
}

void MainRenderer::updateSize(uivec2 s)
{
    lock();
    if(_currentSize != s)
    {
        _newSize = true;
        _currentSize = s;
        for(int i=0 ; i<NB_SCENE ; ++i)
            _view[i].camera.ratio = float(s.x()) / s.y();
    }
    unlock();
}

void MainRenderer::resize()
{
    if(_newSize)
    {
        openGL.resetStates();
        _pipeline.create(_currentSize, _renderingParameter);
    }
    _newSize=false;
}

void MainRenderer::addEvent(std::function<void()> f)
{
    _eventMutex.lock();
    _events.push(f);
    _eventMutex.unlock();
}

}

