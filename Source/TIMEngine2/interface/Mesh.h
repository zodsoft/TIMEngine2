#ifndef MESH_H
#define MESH_H

#include "core.h"
#include "renderer/renderer.h"
#include "renderer/DrawState.h"
#include "interface/Geometry.h"
#include "interface/Texture.h"

#include "MemoryLoggerOn.h"
namespace tim
{
    using namespace core;
namespace interface
{
    class Mesh
    {
    public:

        class Element
        {
        public:
            Element();
            Element(const Geometry& g);
            Element(const Geometry& g, float roughness, float metallic, const vec4& color = vec4::construct(1), float specular=0.5);

            Element(const Element& e) { *this = e; }
            Element& operator=(const Element&);

            void setRougness(float r) { _mat.parameter[0] = r; }
            void setMetallic(float m) { _mat.parameter[1] = m; }
            void setSpecular(float s) { _mat.parameter[2] = s; }
            void setColor(const vec4& c) { _mat.color = c; }

            float roughness() const { return _mat.parameter[0]; }
            float metallic() const { return _mat.parameter[1]; }
            float specular() const { return _mat.parameter[2]; }
            vec4 color() const { return _mat.color; }

            void setGeometry(const Geometry& g) { _geometry = g; }
            const Geometry& geometry() const { return _geometry; }

            void setTexture(const Texture& t, uint index);
            Texture texture(uint index) const { index = std::min(index,2u); return _textures[index]; }

            renderer::DrawState& drawState() { return _state; }
            const renderer::DrawState& drawState() const { return _state; }

            const renderer::Material& internalMaterial() const { return _mat; }
            renderer::Material& internalMaterial() { return _mat; }

        private:
            Geometry _geometry;
            Texture _textures[3];
            renderer::Material _mat;
            renderer::DrawState _state;

            void setDefault();
            void flushMat();
        };

        Mesh() = default;
        Mesh(const Element& e)
        {
            _elements.push_back(e);
            _initialVolume = e.geometry().volume();
        }

        Mesh(const Mesh&) = default;
        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;
        Mesh& operator=(const Mesh&) = default;

        ~Mesh() = default;

        Mesh& addElement(const Element& e)
        {
            if(_elements.empty())
                _initialVolume = e.geometry().volume();
            else
                _initialVolume = _initialVolume.max(e.geometry().volume());

            _elements.push_back(e);
            return *this;
        }

        Mesh& clear()
        {
            _elements.clear();
            _initialVolume = Sphere();
            return *this;
        }

        Element& element(uint index) { return _elements[index]; }
        const Element& element(uint index) const { return _elements[index]; }

        bool empty() const { return _elements.empty(); }
        uint nbElements() const { return _elements.size(); }

        const Sphere& initialVolume() const { return _initialVolume; }

    private: 
        vector<Element> _elements;
        Sphere _initialVolume;
    };
}
}
#include "MemoryLoggerOff.h"

#endif // MESH_H