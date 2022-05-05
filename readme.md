In popen2.cc, we can incorperate `pclose2()` to the destructor of
`subprocess_t` in C++ (given we need to copy the subprocess returned
from `popen2()`, we may need to add a shared pointer).

```cpp
class PopenedSubprocess {
    shared_ptr<subprocess_t> p {}; // shared pointer to the process
public:
    PopenedSubprocess(const char* program) {
        p = make_shared<subprocess_t>(popen2(program));
    }

    ~PopenedSubprocess() {
        if (p.use_count() == 1) // the last one
            pclose2(p.get());
    }
};
```
