#ifndef EPICSGLOBALALLOCATOR_HPP
#define EPICSGLOBALALLOCATOR_HPP

namespace epics {
  namespace allocator {
    template <class Allocator>
    class global
    {
    public:
      typedef Allocator value_type;

      static Allocator &instance() {
        static Allocator in;
        return in;
      }
    };
  }
}

#endif // EPICSGLOBALALLOCATOR_HPP