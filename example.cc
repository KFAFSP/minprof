#include <cstdlib>
// EXIT_SUCCESS
// std::atexit

#include <iostream>
// std::cout

#include "minprof.hh"
// MINPROF_TIMED
// MINPROF_SECTION
// MINPROF_DUMP

using namespace std;

void test1()
{
    // You can trigger a single event manually:
    MINPROF_EVENT("all|C");

    // You can also have it be triggered as part of a section you'd like to track:
    /* Sections always have two counters: number of invocations and total time.
     *
     * In this case, the following counters are added:
     *      test1|C -> Increments on every section entry.
     *      test1|T -> Increments by nanoseconds spent in section.
     *
     * You should consider to sticking to this notation of suffixing the counter type.
     */
    MINPROF_SECTION("test1") {
        cout << "test1!";
        cout << endl;
    }

    // You can also have single statements.
    MINPROF_SECTION("test1");

    // But keep in mind that this does not work right:
/*  if (<cond>)
        MINPROF_SECTION("my_section") {
            foo();
            bar();
        }
    else
        foo_bar(); */
    // Because the macros expand to in if-statement to introduce a new scope, this happens:
/*  if (<cond>)
        if (...MINPROF STUFF THAT ALWAYS RETURNS TRUE...) {
            foo();
            bar();
        }
    ->  else
            foo_bar(); */
    // And the else binds to the inner if. You get the idea...
}

void test2()
{
    // You may also get a StaticCounter as reference yourself using this macro:
    MINPROF_COUNTER("all|C")++;

    // It supports pre and post increment, as well as add-assign:
    auto& test2_C = MINPROF_COUNTER("test2|C");
    test2_C += 9;
    assert(test2_C++ == 9);

    // And you can get a Counter as a timer:
    auto& test2_T = MINPROF_TIMER("test2|T");
    // ...which responds differently, as it is oriented on std::chrono::duration.
    test2_T += std::chrono::milliseconds{1000};
    test2_T += std::chrono::microseconds{200};

    // If you want to incorporate timing in your program logic, consider a Stopwatch:
    minprof::Timer timer{};
    minprof::Stopwatch sw{timer};

    // It can start...
    sw.start();
    // Split the time...
    cout << "Split time: ";
    cout << sw.split().count();
    // ...and stop.
    const auto dur = sw.stop();
    cout << "ns, stop time: " << dur.count() << "ns." << endl;

    // All while increasing the Timer/Counter.
    assert(timer.value().count() > 0);
}

void tight()
{
    // This is how a Counter increase happens internally:
    ++::minprof::StaticCounter<typestring_is("all|C")>::get();

    // So lets time at a tight loop:
    MINPROF_TIMED("MILLION_EVENTS|T") {
        for (unsigned i = 0; i < 1000000; ++i)
            MINPROF_EVENT("MILLION_EVENTS|C");
    }

    // ...and a tight loop of sections:
    {
        // This how timed scopes work btw.
        minprof::Scopewatch sw{MINPROF_TIMER("MILLION_SECTIONS|T")};

        for (unsigned i = 0; i < 1000000; ++i)
            MINPROF_SECTION("MILLION_SECTIONS");
    }

    // Inspecting an optimized dump should show you that a counter increment reduces to:
    //
    //      lock addq $0x1,0x0(%rip)
    //
    // Where the zeroes will hold the relocatable address of the counter if you are using multiple
    // translation units (which is totally fine and safe). The lock prefix stems from using atomics.
}

int main(int argc, char* argv[])
{
    cout << "TESTS:" << endl << endl;

    // Simple, preffered usage.
    test1();

    // Some quirky things.
    test2();

    // Internals.
    tight();

    cout << endl << "STATS:" << endl << endl;

    // Let's see...

    const auto event_incs = MINPROF_COUNTER("MILLION_EVENTS|C").value();
    const auto event_time = MINPROF_TIMER("MILLION_EVENTS|T").value();
    cout << "Event increase takes " << event_time.count() / event_incs << "ns" << endl;

    const auto sect_entrys = MINPROF_COUNTER("MILLION_SECTIONS|C").value();
    const auto sect_time = MINPROF_TIMER("MILLION_SECTIONS|T").value();
    cout << "Section entry takes  " << sect_time.count() / sect_entrys << "ns" << endl;

    cout << endl << "DUMP:" << endl << endl;

    // This will dump all counters as CSV to console.
    MINPROF_DUMP(cout);
    // This will dump to "myfile.csv" instead.
/*  MINPROF_DUMP("myfile.csv"); */
    // This will dump to the "minprof.csv" default file.
/*  MINPROF_DUMP(); */
    // You can even be fancy and do this, so your program auto-dumps at graceful exit!
/*  atexit(MINPROF_DUMP); */

    return EXIT_SUCCESS;
}