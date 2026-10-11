// SPDX-License-Identifier: MIT
#include "runtime_admission.hpp"
#include <cassert>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>
using native_portal::live::RuntimeAdmission;
int main() {
    RuntimeAdmission a;
    assert(a.phase()==RuntimeAdmission::Phase::Prebind);
    assert(!a.original(7,[]{return true;}));
    assert(a.bind(7));assert(!a.bind(8));
    assert(!a.original(8,[]{assert(false);return true;}));
    bool retained=false;
    assert(a.original(7,[&]{retained=true;return true;}));assert(retained);
    assert(!a.engage(7,[]{assert(false);return true;}));
    assert(!a.begin_close(8));
    assert(a.begin_close(7));
    assert(!a.original(7,[]{assert(false);return true;}));
    assert(!a.finish_close(7));a.original_return(7);assert(a.finish_close(7));
    assert(!a.engage(7,[]{assert(false);return true;}));
    RuntimeAdmission b;assert(b.bind(11));
    assert(!b.original(11,[]{return false;}));
    assert(!b.engage(11,[]{return false;}));
    assert(b.phase()==RuntimeAdmission::Phase::Dormant);
    assert(b.engage(11,[]{return true;}));
    assert(!b.original(11,[]{assert(false);return true;}));
    assert(b.reader(11));assert(b.begin_close(11));assert(!b.reader(11));
    assert(!b.finish_close(11));b.reader_return(11);assert(b.finish_close(11));
    // Real overlap: retain and complete native-original reservation survive
    // close. No callback/owner pump and no sleeps are involved.
    RuntimeAdmission c;assert(c.bind(13));
    std::mutex m;std::condition_variable cv;bool started=false,done=false;
    std::thread caller([&]{
        assert(c.original(13,[]{return true;}));
        {std::unique_lock<std::mutex> lock(m);started=true;cv.notify_all();cv.wait(lock,[&]{return done;});}
        c.original_return(13);
    });
    {std::unique_lock<std::mutex> lock(m);cv.wait(lock,[&]{return started;});}
    assert(!c.engage(13,[]{assert(false);return true;}));
    assert(c.begin_close(13));assert(!c.finish_close(13));
    {std::lock_guard<std::mutex> lock(m);done=true;cv.notify_all();}caller.join();
    assert(c.finish_close(13));
    RuntimeAdmission d;assert(d.bind(17));d.invalidate();
    assert(!d.original(17,[]{assert(false);return true;}));
    assert(!d.engage(17,[]{assert(false);return true;}));
    // ENGAGED never returns to DORMANT, including a refused reader or close.
    RuntimeAdmission e;assert(e.bind(19));assert(e.engage(19,[]{return true;}));
    assert(e.engage(19,[]{assert(false);return false;}));
    assert(!e.reader(20));assert(e.phase()==RuntimeAdmission::Phase::Engaged);
    RuntimeAdmission full;assert(full.bind(21));
    for(unsigned i=0;i<64;++i)assert(full.original(21,[]{return true;}));
    assert(!full.original(21,[]{assert(false);return true;}));
    for(unsigned i=0;i<64;++i)full.original_return(21);
    assert(full.engage(21,[]{return true;}));
    // A real racing first engagement and original have exactly one winner.
    for(unsigned i=0;i<64;++i) {
        RuntimeAdmission race;assert(race.bind(23));
        std::atomic<bool> start{false},release{false};bool admitted=false;
        std::thread contender([&]{
            while(!start.load())std::this_thread::yield();
            admitted=race.original(23,[]{return true;});
            while(!release.load())std::this_thread::yield();
            if(admitted)race.original_return(23);
        });
        start=true;const bool engaged=race.engage(23,[]{return true;});
        release=true;contender.join();assert(engaged!=admitted);
    }
    // Reject the inherited process before any inherited mutex operation.
    const auto child=fork();assert(child>=0);
    if(!child){assert(!d.bind(18));assert(!d.reader(17));_exit(0);}
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
}
