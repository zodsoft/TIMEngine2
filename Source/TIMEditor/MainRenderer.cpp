#include "MainRenderer.h"
#include "Rand.h"
#include <QThread>
#include <QModelIndex>
#include <QElapsedTimer>
#include <QWaitCondition>

#include "MemoryLoggerOn.h"
namespace tim{

#undef interface
#undef DrawState
using namespace core;
using namespace renderer;
using namespace resource;
using namespace interface;

MainRenderer::MainRenderer(RendererWidget* parent) : _parent(parent)
{
    _renderingParameter.useShadow = true;
    _renderingParameter.shadowCascad = {4,15,50};
    _renderingParameter.shadowResolution = 2048;
    _renderingParameter.usePointLight = false;
    _currentSize = {200,200};
    _newSize = true;

    _running = true;

    _view[0].camera.ratio = 1;
    _view[0].camera.clipDist = {.1,100};
    _view[0].camera.pos = {0,-3,0};
    _view[0].camera.dir = {0,0,0};

    _view[1].camera.ratio = 1;
    _view[1].camera.clipDist = {.1,1000};
    _view[1].camera.fov = 110;

    _scene[1].globalLight.dirLights.push_back({vec3(1,1,-2), vec4(1,1,1,1), true});
    _dirLightView[1].dirLightView.lightDir = vec3(1,1,-2);

}

void MainRenderer::main()
{
    lock();

    ShaderPool::instance().add("gPass", "shader/gBufferPass.vert", "shader/gBufferPass.frag").value();
    ShaderPool::instance().add("highlighted", "shader/overlayObject.vert", "shader/overlayObject.frag").value();
    ShaderPool::instance().add("highlightedMoving", "shader/overlayObject.vert", "shader/overlayObject2.frag").value();
    ShaderPool::instance().add("fxaa", "shader/fxaa.vert", "shader/fxaa.frag").value();

    {
    const float lineLength = 1000;
    VNCT_Vertex vDataX[3] = {{vec3(-lineLength,0,0),vec3(),vec2(),vec3()},
                             {vec3(0    ,0,0),vec3(),vec2(),vec3()},
                             {vec3(lineLength ,0,0),vec3(),vec2(),vec3()}};

    VNCT_Vertex vDataY[3] = {{vec3(0,-lineLength,0),vec3(),vec2(),vec3()},
                             {vec3(0,0    ,0),vec3(),vec2(),vec3()},
                             {vec3(0,lineLength ,0),vec3(),vec2(),vec3()}};

    VNCT_Vertex vDataZ[3] = {{vec3(0,0,-lineLength),vec3(),vec2(),vec3()},
                             {vec3(0,0,0    ),vec3(),vec2(),vec3()},
                             {vec3(0,0,lineLength ),vec3(),vec2(),vec3()}};

    uint iData[6] = {0,1,2};

    VBuffer* tmpVB = renderer::vertexBufferPool->alloc(3);
    IBuffer* tmpIB = renderer::indexBufferPool->alloc(3);
    tmpVB->flush(reinterpret_cast<float*>(vDataX),0,3);
    tmpIB->flush(iData,0,3);

    _lineMesh[0] = Mesh(Mesh::Element(Geometry(new MeshBuffers(tmpVB,tmpIB, Sphere(vec3(), lineLength)))));

    tmpVB = renderer::vertexBufferPool->alloc(3);
    tmpIB = renderer::indexBufferPool->alloc(3);
    tmpVB->flush(reinterpret_cast<float*>(vDataY),0,3);
    tmpIB->flush(iData,0,3);

    _lineMesh[1] = Mesh(Mesh::Element(Geometry(new MeshBuffers(tmpVB,tmpIB,Sphere(vec3(), lineLength)))));

    tmpVB = renderer::vertexBufferPool->alloc(3);
    tmpIB = renderer::indexBufferPool->alloc(3);
    tmpVB->flush(reinterpret_cast<float*>(vDataZ),0,3);
    tmpIB->flush(iData,0,3);

    _lineMesh[2] = Mesh(Mesh::Element(Geometry(new MeshBuffers(tmpVB,tmpIB,Sphere(vec3(), lineLength)))));

    for(int i=0 ; i<3 ; ++i)
    {
        vec4 col; col[i] = 1;
        _lineMesh[i].element(0).setColor(col);
        _lineMesh[i].element(0).setSpecular(0);
        _lineMesh[i].element(0).setRoughness(1);
        _lineMesh[i].element(0).setEmissive(0.2);
        _lineMesh[i].element(0).drawState().setShader(ShaderPool::instance().get("gPass"));
        _lineMesh[i].element(0).drawState().setPrimitive(DrawState::LINE_STRIP);
    }

//    DrawState s1, s2;
//    s1.setPrimitive(DrawState::LINES);
//    s2.setPrimitive(DrawState::TRIANGLES);

//    s1.debug();
//    s2.debug();
//    std::cout << "Comp:" << (s1==s2) << std::endl;
//    std::cout << "size:" << sizeof(DrawState) << std::endl;
    }

    resize();

    QList<QString> skybox;
    skybox += "skybox/simple4/x.png";
    skybox += "skybox/simple4/nx.png";
    skybox += "skybox/simple4/y.png";
    skybox += "skybox/simple4/ny.png";
    skybox += "skybox/simple4/z.png";
    skybox += "skybox/simple4/nz.png";

    setSkybox(0, skybox);
    setSkybox(1, skybox);

    unlock();
    while(_running)
    {
        lock();

        QElapsedTimer timer;
        timer.start();

        _eventMutex.lock();
        while(!_events.empty())
            _events.dequeue()();

        _waitNoEvent.wakeAll();
        _eventMutex.unlock();

        updateCamera_SceneEditor();

        if(_pipeline.pipeline)
        {
            _pipeline.setScene(_scene[_curScene], _view[_curScene]);
            if(_scene[_curScene].globalLight.dirLights.size() > 0 && _scene[_curScene].globalLight.dirLights[0].projectShadow)
            {
                _dirLightView[_curScene].dirLightView.camPos = _view[_curScene].camera.pos;
                _dirLightView[_curScene].dirLightView.lightDir = _scene[_curScene].globalLight.dirLights[0].direction;
                _pipeline.setDirLightView(_dirLightView[_curScene]);
            }

            _pipeline.pipeline->prepare();
            _pipeline.pipeline->render();
        }

        resize();

        GL_ASSERT();
        _parent->swapBuffers();

        _time = float(std::max(timer.elapsed(), qint64(1))) / 1000;

        unlock();
    }
}

void MainRenderer::waitNoEvent()
{
    _eventMutex.lock();
    _waitNoEvent.wait(&_eventMutex);
    _eventMutex.unlock();
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
        _pipeline.rendererEntity->envLightRenderer().setEnableGI(true);
        _pipeline.setScene(_scene[0], _view[0]);
        _pipeline.rendererEntity->envLightRenderer().setSkybox(_scene[0].globalLight.skybox.first, _scene[0].globalLight.skybox.second);
    }
    _newSize=false;
}

void MainRenderer::updateCamera_SceneEditor()
{
    if(_curScene != 1 || !_enableMove) return;

    vec3 newPos    = _view[1].camera.pos,
         newRelDir = _view[1].camera.dir - _view[1].camera.pos;

    vec3 forward = newRelDir.normalized() * _time * (_speedBoost ? 10:2);
    vec3 side    = newRelDir.cross(-_view[1].camera.up).normalized() * _time * (_speedBoost ? 10:2);

    if(_moveDirection[0])
        newPos += forward;
    else if(_moveDirection[2])
        newPos -= forward;

    if(_moveDirection[1])
        newPos += side;
    else if(_moveDirection[3])
        newPos -= side;

    _view[1].camera.pos = newPos;
    _view[1].camera.dir = _view[1].camera.pos + newRelDir;
}

void MainRenderer::addEvent(std::function<void()> f)
{
    _eventMutex.lock();
    _events.append(f);
    _eventMutex.unlock();
}

void MainRenderer::updateCamera_MeshEditor(int wheel)
{
    lock();
    if(_curScene == 0)
    {
        float dist = _view[0].camera.pos.length();
        dist += 0.05 * float(wheel) *
                (std::max(_time, 0.001f) * fabs(dist));
        dist = dist < 0.1 ? 0.1 : dist;
        dist = dist > 100 ? 100 : dist;

        _view[0].camera.pos.resize(dist);
    }
    unlock();
}

void MainRenderer::updateCamera_SceneEditor(int wheel)
{
    lock();
    if(_curScene > 0)
    {
        float dist = 0.1 * float(wheel) * std::max(_time, 0.001f);
        if(_speedBoost)
            dist *= 0.1;

        _view[_curScene].camera.pos.z() += dist;
        _view[_curScene].camera.dir.z() += dist;
    }
    unlock();
}

void MainRenderer::setSkybox(int sceneIndex, QList<QString> list)
{
    resource::TextureLoader::ImageFormat formatTex;
    vector<std::string> imgSkybox(6);
    for(int i=0 ; i<list.size() ; ++i)
        imgSkybox[i] = list[i].toStdString();

    vector<ubyte*> dataSkybox = resource::textureLoader->loadImageCube(imgSkybox, formatTex);

    renderer::Texture::GenTexParam skyboxParam;
    skyboxParam.format = renderer::Texture::RGBA8;
    skyboxParam.nbLevels = 1;
    skyboxParam.size = uivec3(formatTex.size,0);
    renderer::Texture* skybox = renderer::Texture::genTextureCube(skyboxParam, dataSkybox, 4);
    renderer::Texture* processedSky =
            renderer::IndirectLightRenderer::processSkybox(skybox,
            pipeline().rendererEntity->envLightRenderer().processSkyboxShader());

    for(uint j=0 ; j<dataSkybox.size() ; ++j)
        delete[] dataSkybox[j];

    getScene(sceneIndex).globalLight.skybox = {skybox, processedSky};
}

}

