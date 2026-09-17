/** \file wccIndiRate.hpp
 * \brief Rate limiting for INDI traffic.
 * \author Adam Schilperoort
 *
 * The INDI server serializes every property update through a single set of FIFOs,
 * so a handful of applications each publishing at tens of hertz will saturate it
 * and start dropping devices. The WCC applications therefore hold all of their
 * INDI reads and writes to 1 Hz.
 *
 * That is a real constraint on the control loops, not just on status reporting: it
 * puts a 1 s floor on the fast guiding loop's correction interval. The loops are
 * written with a configurable period which is clamped to this floor rather than
 * silently exceeding it, so the limit is visible in the configuration instead of
 * buried.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccIndiRate_hpp
#define wccIndiRate_hpp

namespace MagAOX
{
namespace wcc
{

/// The INDI update interval every WCC application holds itself to [s].
/** One hertz. Anything faster risks overloading the INDI server.
 *
 * \ingroup wccCommon
 */
constexpr double indiMinPeriod = 1.0;

/// The same interval expressed as a MagAOXApp loop pause [ns].
/** Assigning this to `m_loopPause` makes appLogic, and therefore the status
 * publishing it drives, run at the allowed rate.
 *
 * \ingroup wccCommon
 */
constexpr unsigned long indiLoopPause = 1000000000UL;

/// Clamp a requested period to the INDI rate limit.
/** \returns the period to actually use [s]
 *
 * \ingroup wccCommon
 */
inline double clampIndiPeriod( double requested /**< [in] requested interval [s] */ )
{
    return ( requested < indiMinPeriod ) ? indiMinPeriod : requested;
}

/// A gate that allows an action no more often than a given interval.
/** Used to hold outbound INDI traffic to the allowed rate from code paths that
 * are called more often than that, such as a worker thread loop.
 *
 * Not thread safe. Give each thread that needs one its own.
 *
 * \ingroup wccCommon
 */
class rateGate
{
    /** \name Gate State - Data
     *@{
     */
  protected:
    double m_period{ indiMinPeriod }; ///< Minimum interval between allowed actions [s].

    double m_last{ -1e300 }; ///< Time the last action was allowed [s]. Far past so the first call passes.
    ///@}

  public:
    /// Construct with an interval, clamped to the INDI rate limit.
    explicit rateGate( double period = indiMinPeriod /**< [in] minimum interval [s] */ );

    /// Set the interval, clamped to the INDI rate limit.
    void period( double p /**< [in] minimum interval [s] */ );

    /// The interval in use [s].
    double period() const;

    /// Test whether enough time has passed, and if so record this as the action time.
    /** \returns true if the action is allowed now
     * \returns false if it is too soon
     */
    bool check( double now /**< [in] current time [s], from mx::sys::get_curr_time() */ );

    /// Time remaining until the next action is allowed [s].
    /** \returns the wait, or 0 if an action is allowed now
     */
    double remaining( double now /**< [in] current time [s] */ ) const;

    /// Forget the last action time, so the next check() passes.
    void reset();
};

inline rateGate::rateGate( double period )
{
    m_period = clampIndiPeriod( period );
}

inline void rateGate::period( double p )
{
    m_period = clampIndiPeriod( p );
}

inline double rateGate::period() const
{
    return m_period;
}

inline bool rateGate::check( double now )
{
    if( now - m_last < m_period )
    {
        return false;
    }

    m_last = now;

    return true;
}

inline double rateGate::remaining( double now ) const
{
    const double r = m_period - ( now - m_last );

    return ( r > 0 ) ? r : 0.0;
}

inline void rateGate::reset()
{
    m_last = -1e300;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccIndiRate_hpp
