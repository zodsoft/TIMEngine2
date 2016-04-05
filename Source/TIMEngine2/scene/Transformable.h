#ifndef TRANSFORMABLE_H_INCLUDED
#define TRANSFORMABLE_H_INCLUDED

#include "core.h"
#include "SceneContainer.h"

#include "MemoryLoggerOn.h"
namespace tim
{
    using namespace core;
namespace scene
{
    class Transformable : boost::noncopyable
    {
        friend class SceneContainer<Transformable>;

    public:
        const Sphere& volume() const { return _volume; }

    protected:
        void setVolume(const Sphere& s) { _volume = s; }

        Transformable() = default;
        virtual ~Transformable() = 0;

    private:
        Sphere _volume;

        mutable SceneContainer<Transformable>::TransformableInfo _containerInfo;
    };

    inline Transformable::~Transformable() {}
}
}
#include "MemoryLoggerOff.h"


#endif // TRANSFORMABLE_H_INCLUDED