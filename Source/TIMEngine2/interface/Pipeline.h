#ifndef PIPELINE_H
#define PIPELINE_H

#include <boost/thread/locks.hpp>
#include "SpinLock.h"
#include "core.h"
#include "Camera.h"
#include "renderer/renderer.h"
#include "renderer/Texture.h"
#include "renderer/MeshRenderer.h"
#include "renderer/DirectionalLightRenderer.h"
#include "renderer/IndirectLightRenderer.h"
#include "renderer/TiledLightRenderer.h"
#include "renderer/PostReflexionRenderer.h"
#include "MeshInstance.h"
#include "LightInstance.h"

#include "MemoryLoggerOn.h"
namespace tim
{
    using namespace core;
namespace interface
{
    class Pipeline
    {
    public:

        /** Entity Node **/

        struct DirectionalLight
        {
            vec3 direction; vec4 color;
            bool projectShadow;
        };

        struct GlobalLight
        {
             vector<DirectionalLight> dirLights;
             std::pair<renderer::Texture*, renderer::Texture*> skybox = {nullptr,nullptr};
        };

        template<class SceneType>
        struct SceneEntity : boost::noncopyable
        {
            SceneType scene;
            GlobalLight globalLight;
        };

        struct SceneView
        {
            Camera camera;
            DirLightView dirLightView;
        };

        class DeferredRendererEntity
        {
            friend class Pipeline;
            friend class std::default_delete<DeferredRendererEntity>;

        public:
            renderer::DeferredRenderer& deferredRenderer() { return _renderer; }
            renderer::TiledLightRenderer& lightRenderer() { return _lightContext; }
            renderer::DirectionalLightRenderer& dirLightRenderer() { return _dirLightRenderer; }
            renderer::IndirectLightRenderer& envLightRenderer() { return _envLightRenderer; }
            renderer::PostReflexionRenderer* reflexionRenderer() { return _reflexionRenderer; }

        private:
            renderer::DeferredRenderer _renderer;
            renderer::TiledLightRenderer _lightContext;
            renderer::DirectionalLightRenderer _dirLightRenderer;
            renderer::IndirectLightRenderer _envLightRenderer;
            renderer::PostReflexionRenderer* _reflexionRenderer = nullptr;

            DeferredRendererEntity(const uivec2& res, const renderer::FrameParameter& param)
                : _renderer(res, param), _lightContext(_renderer),
                  _dirLightRenderer(_lightContext), _envLightRenderer(_lightContext)
                  /*,_reflexionRenderer(new renderer::PostReflexionRenderer(_lightContext))*/ {}

            ~DeferredRendererEntity()
            {
                delete _reflexionRenderer;
            }
        };


        /*****************/
        /** Process node */
        /*****************/

        class ProcessNode
        {
            friend class Pipeline;
        public:
            virtual void prepare() = 0;

        protected:
            virtual void reset() { _alreadyPrepared = false; }
            virtual ~ProcessNode() {}

            mutable SpinLock _preparedLock;
            mutable bool _alreadyPrepared = false;

            bool tryPrepare() const
            {
                boost::lock_guard<SpinLock> guard(_preparedLock);
                if(_alreadyPrepared) return false;
                else
                {
                    _alreadyPrepared = true;
                    return true;
                }
            }

        };

        template<class Type>
        class CollectObjectNode : public ProcessNode
        {
        public : virtual const vector<std::reference_wrapper<Type>>& get(int index = 0) const = 0;
        };

        class RendererNode : virtual public ProcessNode
        {
        public:
            virtual void render() = 0;

        protected:
            mutable SpinLock _renderedlock;
            mutable bool _alreadyRendered = false;

            void reset() override { ProcessNode::reset(); _alreadyRendered=false; }

            bool tryRender() const
            {
                boost::lock_guard<SpinLock> guard(_renderedlock);
                if(_alreadyRendered) return false;
                else
                {
                    _alreadyRendered = true;
                    return true;
                }
            }
        };

        class OutBufferNode : public RendererNode
        {
        public: virtual renderer::Texture* buffer() const = 0;
        };

        class OutBuffersNode : public RendererNode
        {
        public:
            OutBufferNode* outputNode(uint index) const
            {
                if(index >= _innerOutput.size()) _innerOutput.resize(index+1, nullptr);

                if(!_innerOutput[index])
                    _innerOutput[index] = new InnerOutBufferNode(const_cast<OutBuffersNode*>(this), index);
                return _innerOutput[index];
            }

            virtual renderer::Texture* buffer(uint) const = 0;

        protected:
            class InnerOutBufferNode : public OutBufferNode
            {
                friend class OutBuffersNode;

                InnerOutBufferNode(OutBuffersNode* parent, uint index) : _parent(parent), _index(index) {}

                void prepare()
                {
                    _parent->prepare();
                }

                void render()
                { 
                    _parent->render();
                }

                renderer::Texture* buffer() const { return _parent->buffer(_index); }

            private:
                OutBuffersNode* _parent;
                uint _index;
            };

            mutable vector<InnerOutBufferNode*> _innerOutput;
        };

        class InBuffersNode : virtual public ProcessNode
        {
        public:
            void setBufferOutputNode(OutBufferNode* node, uint index)
            {
                if(_input.size() <= index)
                    _input.resize(index+1, nullptr);

                _input[index] = node;
            }

            void prepare() override
            {
                if(!tryPrepare()) return;

                for(uint i=0 ; i<_input.size() ; ++i)
                {
                    if(_input[i])
                        _input[i]->prepare();
                }
            }

        protected:
            vector<OutBufferNode*> _input;
        };

        class InOutBufferNode : public InBuffersNode, public OutBufferNode {};

        class DepthMapRendererNode : public OutBuffersNode
        {
        public:
            void addMeshInstanceCollector(CollectObjectNode<MeshInstance>& c) { _meshInstanceSource.push_back(&c); }
            void setSceneView(SceneView& view) { _sceneView = &view; }

            const vector<mat4>& matrix() const
            {
                return _matrix;
            }

        protected:
            vector<CollectObjectNode<MeshInstance>*> _meshInstanceSource = {};
            SceneView* _sceneView = nullptr;

            vector<mat4> _matrix;
        };

        class ObjectRendererNode : public OutBuffersNode
        {
        public:
            void addMeshInstanceCollector(CollectObjectNode<MeshInstance>& c) { _meshInstanceSource.push_back(&c); }
            void addLightInstanceCollector(CollectObjectNode<LightInstance>& c) { _lightInstanceSource.push_back(&c); }
            void setSceneView(SceneView& view) { _sceneView = &view; }
            void setGlobalLight(const GlobalLight& gLight) { _globalLightInfo=&gLight; }

            void setDirLightShadow(DepthMapRendererNode& r, uint index)
            {
                if(_dirLightDepthMapRenderer.size() <= index)
                    _dirLightDepthMapRenderer.resize(index+1);

                _dirLightDepthMapRenderer[index] = &r;
            }

        protected:
            vector<CollectObjectNode<MeshInstance>*> _meshInstanceSource = {};
            vector<CollectObjectNode<LightInstance>*> _lightInstanceSource = {};
            SceneView* _sceneView = nullptr;
            const GlobalLight* _globalLightInfo = nullptr;

            vector<DepthMapRendererNode*> _dirLightDepthMapRenderer;
        };

        class TerminalNode : public InBuffersNode, public RendererNode { };

        /* Methods */
        Pipeline();
        ~Pipeline();

        renderer::MeshRenderer& meshRenderer();

        DeferredRendererEntity& genDeferredRendererEntity(const uivec2&);

        template <class T>
        typename std::enable_if<std::is_default_constructible<T>::value, T>::type& createNode()
        {
            T* node = new T;
            _allProcessNodes.push_back(node);
            return *node;
        }

        template <class T>
        typename std::enable_if<std::is_constructible<T, renderer::MeshRenderer&>::value, T>::type& createNode()
        {
            T* node = new T(_meshRenderer);
            _allProcessNodes.push_back(node);
            return *node;
        }

        template <class T>
        typename std::enable_if<std::is_constructible<T, const renderer::FrameParameter&>::value, T>::type& createNode()
        {
            T* node = new T(_meshRenderer.frameState());
            _allProcessNodes.push_back(node);
            return *node;
        }

        void setOutputNode(TerminalNode&);

        void prepare();
        void render();

    private:
        renderer::MeshRenderer _meshRenderer;

        boost::container::map<uivec2, std::unique_ptr<DeferredRendererEntity>> _deferredRendererEntity;

        vector<ProcessNode*> _allProcessNodes;
        TerminalNode* _outputNode = nullptr;
    };

}
}
#include "MemoryLoggerOff.h"

#endif // PIPELINE_H