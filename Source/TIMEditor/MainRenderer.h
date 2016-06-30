#ifndef MAINRENDERER_H
#define MAINRENDERER_H

#include "interface/FullPipeline.h"
#include "interface/ShaderPool.h"
#include "resource/MeshLoader.h"
#include "resource/AssetManager.h"
#include "interface/pipeline/pipeline.h"

#include <QMutex>
#include <QQueue>
#include <functional>
#include <QWaitCondition>

#include "RendererWidget.h"
#undef interface
#include "MemoryLoggerOn.h"
namespace tim{
class MainRenderer
{
public:
    MainRenderer(RendererWidget*);
    void main();
    float elapsedTime() const { return _time; }

    interface::FullPipeline& pipeline() { return _pipeline; }
    void lock() const { _mutex.lock(); }
    void unlock() const { _mutex.unlock(); }
    void stop() { _running=false; }
    void waitNoEvent();

    void updateSize(uivec2);

    void updateCamera_MeshEditor(int wheel);
    void updateCamera_SceneEditor(int wheel);

    tim::interface::Pipeline::SceneView& getSceneView(int index) { return _view[index]; }

    tim::interface::Pipeline::SceneEntity<tim::interface::SimpleScene>&
        getScene(int index) { return _scene[index]; }

    int getCurSceneIndex() const { return _curScene; }
    void setCurSceneIndex(int index) { _curScene = index; }
    void setMoveDirection(int dir, bool v) { _moveDirection[dir] = v; }
    void setEnableDirection(bool m) { _enableMove = m; }
    void setSpeedBoost(bool b) { _speedBoost = b; }

    void addEvent(std::function<void()>);

    void setSkybox(int sceneIndex, QList<QString>);

    const tim::interface::Mesh& lineMesh(uint index) const { return _lineMesh[index]; }

private:
    RendererWidget* _parent;
    bool _running;
    float _time = 0;

    bool _newSize=false;
    uivec2 _currentSize;

    interface::FullPipeline::Parameter _renderingParameter;
    interface::FullPipeline _pipeline;
    mutable QMutex _mutex;


    /* Shared state */
    const int NB_SCENE=2;
    tim::interface::Pipeline::SceneView _view[2];
    tim::interface::Pipeline::SceneView _dirLightView[2];
    tim::interface::Pipeline::SceneEntity<tim::interface::SimpleScene> _scene[2];

    int _curScene=0;
    bool _enableMove = false;
    bool _moveDirection[4] = {false};
    bool _speedBoost = false;

    mutable QMutex _eventMutex;
    QQueue<std::function<void()>> _events;
    QWaitCondition _waitNoEvent;

    /* gui elements */
    tim::interface::Mesh _lineMesh[3];

    void resize();
    void updateCamera_SceneEditor();
};

}
#include "MemoryLoggerOff.h"

#endif // MAINRENDERER_H
