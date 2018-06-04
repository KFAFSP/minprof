/** \brief Minimal profiler implemented for C++11 and above.
 *
 * Requires the typestring header.
 *
 * \file    minprof.hh
 * \author  Karl Friebel
 * \date    04.06.1997
 */

#ifndef MINPROF_HH_
#define MINPROF_HH_
#pragma once

// Uses the typestring header to provide compile-time constant naming for StaticCounter instances.
#include "typestring.hh"
// irqus::typestring
// typestring_is

#include <cstdint>
// std::uint64_t
#include <cassert>
// assert
#include <cstring>
// std::strcmp

#include <type_traits>
// std::enable_if
// std::is_same
// std::is_convertible

#include <atomic>
// std::atomic

#include <ratio>
// std::nano
#include <chrono>
// std::chrono::duration
// std::chrono::duration_cast
// std::chrono::high_resolution_clock

#include <vector>
// std::vector
#include <iostream>
// std::ostream
#include <fstream>
// std::ofstream

/* Compiler-independent inlining attributes:
 *
 * Correct operation of this library requires certain functions to be inlined at all costs in order
 * to remain branch-free and without any calls during the profiled paths. The macros here provide a
 * standardized way to achieve this.
 */
#if defined(__GNUC__)
// G++ supports attribute.
#define ALWAYS_INLINE   inline __attribute__((always_inline))
#elif defined(__clang__)
// Clang supports attribute.
#define ALWAYS_INLINE   inline __attribute__((always_inline))
#elif defined(_MSC_VER)
// MSVC supports intrinsic.
#define ALWAYS_INLINE   __forceinline
#endif

namespace irqus {

/* Trait for using typestrings:
 *
 * To write more concise code, this trait checks that a provided type is a typestring instanciation.
 */

template<typename T>
struct is_typestring : std::integral_constant<bool, false> {};

template<char... C>
struct is_typestring<typestring<C...>> : std::integral_constant<bool, true> {};

}

/** \brief Minimal profiler namespace.
 *
 * The minimal profiler aims to provide a standard and easy way to conduct minimal profiling in a
 * thread-safe and global manner throught an application. It's main focuses are lightweight timing
 * and ease of use.
 *
 * The profiler exploits static lifetimes to provide global counters based on atomic 64-bit integers
 * that can usually be efficiently incremented on modern architectures. The static registry uses the
 * statically initialized counter types to effortlessly keep track of all instances used throught
 * the application without the need for calling anything or providing an entry-point. All counters
 * are registered on static init time and are therefore presents right from the start of the
 * application.
 *
 * Some defined macros make the use of this library easier by wrapping boilerplate without actually
 * modifying control flow or register use at the target site.
 */
namespace minprof {

/** \brief Atomic 64-bit counter used by the minimal profiler.
 *
 * Provides a simple wrapper around an atomic unsigned 64-bit integer useful for profiling.
 *
 * The interface of this class limits it's use to monotonic behaviour, meaning that the value can
 * only ever be increased.
 */
class Counter {
public:
    /** \brief Type that can hold the value of the Counter. */
    using value_type    = std::uint64_t;
    /** \brief Type that allows for atomic operations on the value. */
    using atomic_type   = std::atomic<value_type>;

public:
    /** \brief Initialize a new Counter at 0.
     *
     * Because of the constexpr modifier, this type becomes eligible for constant initialization.
     */
    constexpr Counter() noexcept
    : Counter{0}
    {}
    /** \brief Initialize a new Counter.
     *
     * \param   [in]    init    Initial value.
     */
    constexpr Counter(value_type init) noexcept
    : m_value{init}
    {}
    /** \brief Copy a Counter.
     *
     * \param   [in]    copy    Counter to copy.
     */
    Counter(const Counter& copy) noexcept
    : Counter{copy.value()}
    {}
    // No move constructor.
    Counter(Counter&&) = delete;

public:
    /** \brief Copy a Counter.
     *
     * \param   [in]    copy    Counter to copy.
     * \return  *this.
     */
    Counter& operator=(const Counter& copy) noexcept
    {
        m_value = copy;
        return *this;
    }
    // No move assignment operator.
    Counter& operator=(Counter&&) = delete;

    /** \brief Get the current value of this Counter.
     *
     * \return  Current Counter value.
     */
    value_type value() const noexcept
    {
        return m_value;
    }
    /** \brief Implicitly get the current Counter value.
     *
     * \return  Current Counter value.
     */
    operator value_type() const noexcept
    {
        return value();
    }

    /** \brief Increment the Counter by 1.
     *
     * \return  Counter value before increment.
     */
    value_type operator++(int) noexcept
    {
        return m_value.fetch_add(1);
    }
    /** \brief Increment the Counter by 1 and get it's previous value.
     *
     * \return  *this.
     */
    Counter& operator++() noexcept
    {
        ++m_value;
        return *this;
    }

    /** \brief Increment the Counter by a specified amount.
     *
     * \param   [in]    amount  Amount to increment by.
     * \return  *this.
     */
    Counter& operator+=(value_type amount) noexcept
    {
        m_value += amount;
        return *this;
    }

private:
    // Internal counter value.
    atomic_type     m_value;
};

/** \brief Write the value of a Counter to a stream.
 *
 * \param   [in,out]    out     Output stream.
 * \param   [in]        c       Counter instance.
 * \return  out.
 */
static std::ostream& operator<<(std::ostream& out, const Counter& c)
{
    return out << c.value();
}

/** \brief Atomic 64-bit nanosecond timer used by the minimal profiler.
 *
 * Timers are specialized Counters that do not add any more data, but have a timing-oriented
 * interface. They use the backing Counter to store nanosecond durations that can only monotonously
 * increase in value.
 *
 * The Timer interface includes overloads for use with the chrono library accepting any durations,
 * but will perform rounding to nanosecond durations. As only positive values can be stored, wrap
 * around occurs on negative values.
 */
class Timer : public Counter {
public:
    /** \brief Interval precision of the Timer. */
    using period    = std::nano;
    /** \brief Duration type used by the Timer. */
    using duration  = std::chrono::duration<value_type, period>;

public:
    /** \brief Initialize a new Timer at 0ns elapsed.
     *
     * Because of the constexpr modifier, this type becomes eligible for constant initialization.
     */
    constexpr Timer() noexcept
    : Timer{duration::zero()}
    {}
    /** \brief Initialize a new Timer.
     *
     * \param   [in]    init    Initial value.
     */
    constexpr Timer(duration init) noexcept
    : Counter{init.count()}
    {}
    /** \brief Initialize a new Timer.
     *
     * If \p init is negative, the behaviour is undefined.
     * Might include possible loss of precision and/or rounding.
     *
     * Does not partake in overload resolution when the duration type is compatible to the native
     * type, i.e. implicitly convertible to duration.
     *
     * \param   [in]    init    Initial value.
     */
    template<
        typename Rep,
        typename Period,
        typename = typename std::enable_if<
            !std::is_convertible<std::chrono::duration<Rep, Period>, duration>::value
        >::type
    >
    Timer(std::chrono::duration<Rep, Period> init)
    : Timer{std::chrono::duration_cast<duration>(init)}
    {
        // CONTRACT: Verify that the value is in fact positive.
        assert(init.count() > 0);
    }
    /** \brief Copy a Timer.
     *
     * \param   [in]    copy    Timer to copy.
     */
    Timer(const Timer& copy) noexcept
    : Counter{copy}
    {}
    // No move constructor.
    Timer(Timer&&) = delete;

public:
    /** \brief Copy a Timer.
     *
     * \param   [in]    copy    Timer to copy.
     * \return  *this.
     */
    Timer& operator=(const Timer& copy) noexcept
    {
        Counter::operator=(copy);
        return *this;
    }
    // No move assignment operator.
    Timer& operator=(Timer&&) = delete;

    /** \brief Get the current value of this Timer.
     *
     * \return  Current Timer value.
     */
    duration value() const noexcept
    {
        return duration{Counter::value()};
    }
    /** \brief Implicitly get the current Timer value.
     *
     * \return  Current Timer value.
     */
    operator duration() const noexcept
    {
        return value();
    }

    /** \brief Increment this timer.
     *
     * \param   [in]    dur     Duration to increment by.
     * \return  *this.
     */
    Timer& operator+=(duration dur) noexcept
    {
        Counter::operator+=(dur.count());
        return *this;
    }
    /** \brief Increment this timer.
     *
     * If \p dur is negative, the behaviour is undefined.
     * Might include possible loss of precision and/or rounding.
     *
     * Does not partake in overload resolution when the duration type is compatible to the native
     * type, i.e. implicitly convertible to duration.
     *
     * \param   [in]    dur     Duration to increment by.
     * \return  *this.
     */
    template<
        typename Rep,
        typename Period,
        typename = typename std::enable_if<
            !std::is_convertible<std::chrono::duration<Rep, Period>, duration>::value
        >::type
    >
    Timer& operator+=(std::chrono::duration<Rep, Period> dur)
    {
        return *this += std::chrono::duration_cast<duration>(dur);
    }
};

/** \brief Write the value of a Timer to a stream.
 *
 * \param   [in,out]    out     Output stream.
 * \param   [in]        t       Timer instance.
 * \return  out.
 */
static std::ostream& operator<<(std::ostream& out, const Timer& t)
{
    // C++20 will support writing duration suffixes to stream.
    // In the meantime, let's just let the default stream op get the count value.
    return out << t.value();
}

/** \brief Static container for a global Counter.
 *
 * By instanciating this template, a global Counter with static storage is created and registered.
 *
 * \tparam  Name    typestring of the Counter's name.
 */
template<typename Name>
class StaticCounter {
public:
    // Assert that a typestring was passed.
    static_assert(irqus::is_typestring<Name>::value, "Name must be a typestring!");

    /** \brief Counter name typestring. */
    using name = Name;
    /** \brief Index of the counter in the static registration vector. */
    static const unsigned index;

public:
    // No (default) constructor.
    StaticCounter() = delete;
    // No copy constructor.
    StaticCounter(const StaticCounter&) = delete;
    // No copy assignment operator.
    StaticCounter& operator=(const StaticCounter&) = delete;
    // No move constructor.
    StaticCounter(StaticCounter&&) = delete;
    // No move assignment operator.
    StaticCounter& operator=(StaticCounter&&) = delete;

    /** \brief Get the global Counter instance.
     *
     * Calls to this function shall always be inlinied. Due to havinge thetrivially, constexpr
     * constructible Counter objects, the compiler will not generate any checks for this scoped
     * static initialization and instead just allocate a zeroed region in the binary and link all
     * occurences to there.
     *
     * \return  Global Counter instance.
     */
    ALWAYS_INLINE static Counter& get() noexcept
    {
        static Counter instance;

        // Necessary to force the static index member to be statically initialized, thus registering
        // the counter in the static registry. (Does not actually happen here.)
        (void)index;

        return instance;
    }
};

/** \brief Static registry for the StaticCounter types instanciated.
 *
 * This class handles the task of keeping track of all used counters. It's only interface is static,
 * and is automatically invoked during static initialization of the StaticCounters themselves.
 */
class StaticCounterRegistry {
public:
    // No copy constructor.
    StaticCounterRegistry(const StaticCounterRegistry&) = delete;
    // No copy assignment operator.
    StaticCounterRegistry& operator=(const StaticCounterRegistry&) = delete;
    // No move constructor.
    StaticCounterRegistry(StaticCounterRegistry&&) = delete;
    // No move assignment operator.
    StaticCounterRegistry& operator=(StaticCounterRegistry&&) = delete;

    /** \brief Register a StaticCounter.
     *
     * \tparam  typestring Name of the StaticCounter.
     *
     * \return  Index within the static registry.
     */
    template<typename Name>
    static unsigned register_counter()
    {
        using StaticCounter = StaticCounter<Name>;
        auto& self = instance();

        self.m_names.push_back(Name::data());
        self.m_instances.push_back(&StaticCounter::get());

        return self.m_instances.size() - 1;
    }

    /** \brief Get the number of StaticCounters registered.
     *
     * \return  Number of registered counters.
     */
    ALWAYS_INLINE static unsigned count() noexcept
    {
        const auto& self = instance();

        return static_cast<unsigned>(self.m_instances.size());
    }
    /** \brief Find a specific registered counter by name.
     *
     * During compile-time, prefer using the StaticCounter<Name>::get() directly.
     *
     * \param   [in]        name    Name to find.
     * \param   [in,out]    idx     Index of the counter.
     *
     * \retval  true    Counter was found.
     * \retval  false   Counter not found.
     */
    ALWAYS_INLINE static bool find(const char* name, unsigned& idx) noexcept
    {
        // TODO: I'd rather have an optional, but that is C++17 or boost.
        const auto& self = instance();

        for (unsigned i = 0; i < self.m_names.size(); ++i) {
            if (std::strcmp(self.m_names[i], name) == 0) {
                idx = i;
                return true;
            }
        }

        return false;
    }
    /** \brief Get the name of a registered counter.
     *
     * \param   [in]    idx Index of the counter.
     *
     * \retval  nullptr \p idx is out of bounds.
     * \returns Name of the counter.
     */
    ALWAYS_INLINE static const char* get_name(unsigned idx)
    {
        const auto& self = instance();

        if (idx >= self.m_names.size()) {
            return nullptr;
        }

        return self.m_names[idx];
    }
    /** \brief Get the a registered counter.
     *
     * \param   [in]    idx Index of the counter.
     *
     * \retval  nullptr \p idx is out of bounds.
     * \returns Pointer to the Counter.
     */
    ALWAYS_INLINE static Counter* get_counter(unsigned idx)
    {
        const auto& self = instance();

        if (idx >= self.m_instances.size()) {
            return nullptr;
        }

        return self.m_instances[idx];
    }

    static void dump(std::ostream& out)
    {
        const auto& self = instance();

        for (unsigned idx = 0; idx < self.m_instances.size(); ++idx) {
            const auto name = self.m_names[idx];
            if (name) {
                out << name;
            } else {
                out << "counter_" << idx;
            }
            out << ", " << self.m_instances[idx]->value() << std::endl;
        }
    }
    static void dump(const char* file_name)
    {
        std::ofstream csv{file_name};
        dump(csv);
    }
    static void dump()
    {
        dump("minprof.csv");
    }

private:
    // Sadly, vectors aren't constexpr.
    StaticCounterRegistry() = default;

    ALWAYS_INLINE static StaticCounterRegistry& instance() noexcept
    {
        // Typical scoped static initialization for the singleton.
        // This cannot be implemented branch-free however, since StaticCounterRegistry is neither
        // eligible for constant nor zero initialization, I believe.
        static StaticCounterRegistry instance;
        return instance;
    }

    // Vector of registered counter's names.
    std::vector<const char *>   m_names;
    // Vector of registered counters.
    std::vector<Counter*>       m_instances;
};

/** \brief Dump all Counters. */
#define MINPROF_DUMP            ::minprof::StaticCounterRegistry::dump

// Initialization of the index field performs the actual static registration.
template<typename Name>
const unsigned StaticCounter<Name>::index = StaticCounterRegistry::register_counter<Name>();

/** \brief Get a StaticCounter by name.
 *
 * \param   name    Name string literal of the StaticCounter.
 */
#define MINPROF_COUNTER(name)   ::minprof::StaticCounter<typestring_is(name)>::get()

/** \brief Trigger an event by name.
 *
 * Will increase the StaticCounter called <name>.
 *
 * \param   name    Name string literal of the StaticCounter.
 */
#define MINPROF_EVENT(name)     do { ++MINPROF_COUNTER(name); } while (0)

/** \brief Get a StaticCounter as a timer.
 *
 * \param   name    Name string literal of the StaticCounter.
 */
#define MINPROF_TIMER(name)     static_cast<::minprof::Timer&>(MINPROF_COUNTER(name))

/** \brief Stopwatch for manually timing on Timers.
 *
 * Stopwatches are adapters for Timers that allow the user to perform measurements and accumulate
 * them in the backing Timer.
 *
 * The interface of the Stopwatch is not safe in all scenarios, as the user has to watch out for
 * unstarted stopwatches when performing retiring operations. Those can yield in extremely large
 * duration values due to the clock epoch being used as a reference point. Always use the Scopewatch
 * if possible.
 *
 * Stopwatches are not threadsafe, but Timers are. Therefore, if you plan to measure from multiple
 * threads, each thread should get it's own Stopwatch instance referencing the same Timer.
 *
 * The Stopwatch is using the std::chrono::high_resolution_clock, but converting from the native
 * duration to the unsigned 64-bit integer nanosecond durations used by the Timer. This might mean
 * loss of precision or rounding (both very unlikely) and is responsible for the missing noexcept
 * guarantee. However, this does not affect the qualitative correctness as conversions are performed
 * by std::chrono::duration_cast just like with the Timer.
 */
class Stopwatch {
public:
    /** \brief Clock used by all Stopwatch instances. */
    struct Clock : std::chrono::high_resolution_clock {};
    /** \brief Type alias for the duration type. */
    using duration      = Timer::duration;
    /** \brief Type alias for the Clock's time point type. */
    using time_point    = Clock::time_point;

    /** \brief Initialize a new Stopwatch.
     *
     * \param   [in,out]    par     Backing Timer.
     * \param   [in]        started If \c true, starts the Stopwatch.
     */
    Stopwatch(Timer& par, bool started = false) noexcept
    : m_par{par}, m_start{}
    {
        if (started) {
            start();
        }
    }

    /** \brief Copy a Stopwatch.
     *
     * \param   [in]    copy    Stopwatch to copy.
     */
    Stopwatch(const Stopwatch& copy) noexcept
    : m_par{copy.m_par}, m_start{copy.m_start}
    {}

    /** \brief Copy a Stopwatch.
     *
     * \param   [in]    copy    Stopwatch to copy.
     * \returns *this.
     */
    Stopwatch& operator=(const Stopwatch& copy) noexcept
    {
        m_par = copy.m_par;
        m_start = copy.m_start;
        return *this;
    }

    // No move constructor.
    Stopwatch(Stopwatch&&) = delete;
    // No move assignment.
    Stopwatch& operator=(Stopwatch&&) = delete;

    /** \brief Start the Stopwatch.
     *
     * If the Stopwatch is already running, the current measurement is abandoned.
     */
    ALWAYS_INLINE void start() noexcept
    {
        m_start = Clock::now();

        // DEBUG: Default-constructed time_point is unique.
        assert(m_start.time_since_epoch().count() > 0);
    }
    /** \brief Split the Stopwatch time.
     *
     * Measures the elapsed duration, retires that to the backing Timer and continues from here
     * without loss of time.
     *
     * Behaviour is undefined if the Stopwatch was not started.
     *
     * \return  Time elapsed since last start() or split() command.
     */
    ALWAYS_INLINE duration split()
    {
        // CONTRACT: Stopwatch is running.
        assert(m_start.time_since_epoch().count() > 0);

        const auto end = Clock::now();
        const auto native_dur = end - m_start;
        const auto dur = std::chrono::duration_cast<duration>(native_dur);

        m_par += dur;
        m_start = end;

        return dur;
    }
    /** \brief Stop the Stopwatch.
     *
     * Measures the elapsed duration and retires that to the backing Timer.
     *
     * Behaviour is undefined if the Stopwatch was not started.
     *
     * \return  Time elapsed since last start() or split() command.
     */
    ALWAYS_INLINE duration stop()
    {
        const auto dur = split();

        m_start = time_point{};
        return dur;
    }

private:
    // Backing Timer.
    Timer&      m_par;
    // Time of last start() or split() command.
    time_point  m_start;
};

/** \brief Scoped Stopwatch for safe use with the minimal profiler.
 *
 * Scopwatches start and stop automatically on construct/destruct and therefore automate the timing
 * by design. Also, there interface prevents incorrect use of the Stopwatch since no invalid
 * operations can be performed by the user.
 *
 * Due to their nature, Scopewatches can neither be copied nor moved as to not introduce incorrect
 * timing behaviour.
 */
class Scopewatch : private Stopwatch {
public:
    /** \brief Initialize and start a new Scopewatch.
     *
     * \param   [in,out]    par     Backing Timer.
     */
    Scopewatch(Timer& par) noexcept
    : Stopwatch{par, true}
    {}
    /** \brief Stop, retire and destroy a Scopewatch. */
    ~Scopewatch()
    {
        stop();
    }

    // No copy constructor.
    Scopewatch(const Scopewatch&) = delete;
    // No copy assignment.
    Scopewatch& operator=(const Scopewatch&) = delete;

    // No move constructor.
    Scopewatch(Scopewatch&&) = delete;
    // No move assignment.
    Scopewatch& operator=(Scopewatch&&) = delete;

    // Hack to make use of if-condition initialization scoping.
    operator bool() const noexcept
    {
        return true;
    }
};

/** \brief Time the following statement (-block).
 *
 * May cause unexpected parsing when used inside a then-block of an if-statement without curly
 * braces that is followed by an else.
 *
 * \param   name    Name string literal of the StaticCounter.
 */
#define MINPROF_TIMED(name)\
if (::minprof::Scopewatch __scopewatch_ ## __LINE__ {MINPROF_TIMER(name)})

/** \brief Section tracker for use with the minimal profiler.
 *
 * Section instances behave like Scopewatches that also increment a Counter on construct, thus
 * keeping track of both the number of times a section was entered as well as the time spent in it
 * in total.
 */
class Section : private Scopewatch {
public:
    /** \brief Initialize, trigger and time a new Section.
     *
     * \param   [in,out]    c   Counter for section.
     * \param   [in,out]    t   Timer for section.
     */
    Section(Counter& c, Timer& t) noexcept
    : Scopewatch{t}
    {
        ++c;
    }

    // No copy constructor.
    Section(const Section&) = delete;
    // No copy assignment.
    Section& operator=(const Section&) = delete;

    // No move constructor.
    Section(Section&&) = delete;
    // No move assignment.
    Section& operator=(Section&&) = delete;

    // Hack to make use of if-condition initialization scoping.
    operator bool() const noexcept
    {
        return true;
    }
};

/** \brief Profile the following statement (-block).
 *
 * Will accumulate the number of invocations in <name>|C and the total time in <name>|T.
 *
 * May cause unexpected parsing when used inside a then-block of an if-statement without curly
 * braces that is followed by an else.
 *
 * \param   name    Name string literal of the section.
 */
#define MINPROF_SECTION(name)\
if (::minprof::Section __section_ ## __LINE__ {MINPROF_COUNTER(name "|C"), MINPROF_TIMER(name "|T")})

}

/* Exemplary usage:
 *
 * Profile a section like this:
 *
 * MINPROF_SECTION("mySection") {
 *      setup();
 *      doStuff();
 *      teardown();
 * }
 *
 * Time a region like this:
 *
 * MINPROF_TIMED("myTimer|T") {
 *      stuff();
 *      moreStuff();
 * }
 *
 * Dump you results to a stream, file or default file like so:
 *
 * MINPROF_DUMP();
 * MINPROF_DUMP("myfile.csv");
 * MINPROF_DUMP(std::cout);
 *
 */

#endif
