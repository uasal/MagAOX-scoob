
#ifndef xqt_pwrChannel_hpp
#define xqt_pwrChannel_hpp

#include <string>

#include <QWidget>
#include <QSlider>
#include <QTimer>

#include <qwt_text_label.h>

namespace xqt 
{

/// A single power channel control widget
/** Contains the text label and the slider bar control for a single power channel.
  * These widgets are themselves intended to be added to a grid layout -- the 
  * pwrChannel widget does not actually manage them.
  */
class pwrChannel : public QWidget
{
<<<<<<< Updated upstream
   Q_OBJECT
   
protected:
   
   std::string m_channelName; ///< The name of this channel
    
   QwtTextLabel* m_channelNameLabel {nullptr}; ///< The widget to display the channel name
   
   QSlider * m_channelSwitch {nullptr}; ///< The widget providing user control
   
   int m_swTarget {-1};
   
   int m_setSwitchState {0}; ///< The last state set by the user.
   
   std::vector<int> m_outlets; ///< The outlets controlled by this channel.
   
   double m_onDelay {0}; ///< The total turn-on delay for this channel (between outlets)
   
   double m_onTimeout {10000}; ///< The turn-ontimeout for this channel, the time to wait for device to update the status before re-enabling the switch.
   
   double m_offDelay {0}; ///> The turn-off delay for this channel (between outlets)
   
   double m_offTimeout {10000}; ///< The turn-off timeout for this channel, the time to wait for device to update the status before re-enabling the switch.
   
   QTimer * m_timer {nullptr}; ///< Timer for tracking timeouts on channel state changes
   
public:

   ///Constructor
   /** Constructs the m_channelNameLabel and m_channelSwitch widgets, sets the palette of m_channelSwitch, an connects
=======
    Q_OBJECT


  protected:
    std::string m_channelName; ///< The name of this channel

    QwtTextLabel *m_channelNameLabel{ nullptr }; ///< The widget to display the channel name

    QSlider *m_channelSwitch{ nullptr }; ///< The widget providing user control

    pwrChState m_swTarget{ pwrChState::Unk };

    pwrChState m_setSwitchState{ pwrChState::Off }; ///< The last state set by the user.

    std::vector<int> m_outlets; ///< The outlets controlled by this channel.

    double m_onDelay{ 0 }; ///< The total turn-on delay for this channel (between outlets)

    double m_onTimeout{ 3000 }; /**< The turn-ontimeout for this channel, the time to wait for device to update the
                                      status before re-enabling the switch.*/

    double m_offDelay{ 0 }; ///> The turn-off delay for this channel (between outlets)

    double m_offTimeout{ 3000 }; /**< The turn-off timeout for this channel, the time to wait for device to update the
                                      status before re-enabling the switch.*/

    bool m_isToggle {false}; ///< Whether this is a toggle switch (true) or a text switch (false).

    QTimer *m_timer{ nullptr }; ///< Timer for tracking timeouts on channel state changes

  public:
    /// Constructor
    /** Constructs the m_channelNameLabel and m_channelSwitch widgets, sets the palette of m_channelSwitch, an connects
>>>>>>> Stashed changes
     * the m_channelSwitch sliderReleased signal to the sliderRelased slot.
     */ 
   pwrChannel( QWidget * parent = nullptr, 
               Qt::WindowFlags flags = Qt::WindowFlags()
             );

   ///Destructor
   virtual ~pwrChannel();
   
   /// Get the channel name
   /**
     * \returns the current value of m_channelName
<<<<<<< Updated upstream
     */ 
   std::string channelName();
   
   /// Set the channel name
   /** Sets m_channelName.
     */ 
   void channelName( const std::string & nname /**< [in] the new channel name*/);
   
   int switchState();
   
   void switchTarget( int swstate);
   
   void switchState( int swstate);

   QwtTextLabel * channelNameLabel();
   
   QSlider * channelSwitch();
   
   void outlets( const std::vector<int> & outs );
   
   void onDelay( double onD );
   
   void offDelay (double offD);
   
   void calcOnTimeout();
   
   void calcOffTimeout();
   
public slots:
   
   void sliderReleased();
   
   void noTimeOut();
   
   void timeOut();
   
signals:
   
   void switchOn( const std::string & channelName );
   
   void switchOff( const std::string & channelName );
   
   void switchTargetReached();
   
=======
     */
    std::string channelName();

    /// Set the channel name
    /** Sets m_channelName.
     */
    void channelName( const std::string &nname /**< [in] the new channel name*/ );

    int switchState();

    void switchTarget( pwrChState swstate );

    void switchState( pwrChState swstate );

    QwtTextLabel *channelNameLabel();

    QSlider *channelSwitch();

    void outlets( const std::vector<int> &outs );

    void onDelay( double onD );

    void offDelay( double offD );

    void calcOnTimeout();

    void calcOffTimeout();

    void isToggle(bool it)
    {
        m_isToggle = it;
    }

    bool isToggle()
    {
        return m_isToggle;
    }

  public slots:

    void sliderReleased();

    void noTimeOut();

    void timeOut();

  signals:

    void switchOn( const std::string &channelName );

    void switchOff( const std::string &channelName );

    void switchTargetReached();
>>>>>>> Stashed changes
};
   
}//namespace xqt
#endif //xqt_pwrChannel_hpp
