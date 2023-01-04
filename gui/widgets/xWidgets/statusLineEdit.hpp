#ifndef statusLineEdit_hpp
#define statusLineEdit_hpp

#include <iostream>

#include <QLineEdit>
#include <QTimer>
#include <QStyle>
#include <QKeyEvent>

#include "xWidget.hpp"

namespace xqt
{

/// Implements a simultaneous status display and value entry from QLineEdit
/** This widget has three modes: status; status changed; and editing. They are managed according to:
  * - Status is the normal mode, when not being edited or not changed, with text color set by the style sheet.
  * - When the text is changed via setTextChanged the widget adopts the statusChanged style (border highlight) for the change timeout (default 1.5 sec).
  * - Once this widget gets focus, the style is changed to the default text color and enters editing mode.
  * - When in editing mode (has focus), text updates via setTextChanged() will only occur in the background without changing the displayed text.
  * - If the user presses ESC editing is canceled and the widget returns to status mode.
  * - If the user pauses editing for more than the edit timeout (default 10 sec), this returns to status mode with the current value
  * - If focus then returns, the last edited value is loaded
  * - After the stale timeout (default 60 sec), the edited value is cleared so that subsequent edits start with the current value
  * - onReturnPressed should normally be used to signal a new value to set, rather than editingFinished.
  * 
  * To trigger the statusChanged style, the member function setTextChanged() must be called instead of setText().  This will also prevent interrupting
  * current editing when the widget has focus.
  * 
  * Ref: https://doc.qt.io/qt-5/qlineedit.html
  */ 
class statusLineEdit : public QLineEdit
{
   Q_OBJECT
   
public:

   enum valchanges{NOTCHANGED, CHANGED, CHANGED_TIMEOUT};
   enum editchanges{NOTEDITING, STARTED, STOPPED};

protected:

   QString m_currText; ///< The current text, maintained constant while editing is in progress and until a new value is set with setTextChanged.
   QString m_editText; ///< The text as edited, maintained through an edit timeout so that it is restored if editing starts again before the stale timeout.

   bool m_readOnly {false};
   bool m_highlightChanges {true};

   int m_valChanged {NOTCHANGED}; ///< Whether or not the value has changed, can take the values in enum statusLineEdit::valchanges.
   
   int m_editing {NOTEDITING}; ///< Whether or not editing is in progress, can take the values in enum statusLineEdit::editchanges.

   /// The timer for restoring status style from the statusChanged style
   /** This is started for m_changeTimeout msecs after a setTextChanged() is called.
     * 
     * Ref: https://doc.qt.io/qt-5/qtimer.html
     */
   QTimer * m_changeTimer {nullptr};

   std::chrono::milliseconds m_changeTimeout {1500}; ///< The timeout for m_changeTimer, default is 1,500 msec.

   /// The timer for returning to status display mode when editing has paused
   /** This is started for m_editTimeout msecs after a focusInEvent and any keyPressEvent other than ESC.
     * 
     * Ref: https://doc.qt.io/qt-5/qtimer.html
     */
   QTimer * m_editTimer {nullptr};

   std::chrono::milliseconds m_editTimeout {10000}; ///< The timeout for m_editTimer, default is 10,000 msec.

   /// The timer for clearing the edit text after a long pause in editing
   /** This is started for m_staleTimeout msecs after a timeout from m_editTimer.
     * 
     * Ref: https://doc.qt.io/qt-5/qtimer.html
     */
   QTimer * m_staleTimer {nullptr};

   std::chrono::milliseconds m_staleTimeout {60000}; ///< The timeout for m_staleTimer, default is 60,000 msec.

public:

   /// Default constructor.
   statusLineEdit(QWidget *parent = nullptr);

   /// Get the current text
   /** Is independent of the edited text
     *
     * \returns m_currText;
     */ 
   QString currText();

   /// Get the edited text
   /** Is independent of the current text
     *
     * \returns m_editText;
     */
   QString editText();

   /// Set the read only flag 
   /** If set to true, the widget will not accept focus and will not enter edit mode.
     * The default is false.
     * 
     * This sets m_readOnly
     */
   void readOnly(bool ro /**< [in] the new value of the read only flag*/);

   /// Get the value of the read only flag
   /**
     * \returns the current value of m_readOnly
     */ 
   bool readOnly();

   /// Set the highlight changes flag
   /** If set to false, the widget will not change to statusChanged.
     * The default is true.
     * 
     * This sets m_highlightChanges
     */
   void highlightChanges(bool hc);

   /// Get the value of the highlight changes flag
   /**
     * \returns the current value of m_highlightChanges
     */
   bool highlightChanges();

   /// Set the change timeout
   /** The change timeout (m_changeTimeout) is the duration for which the 
     * statusChanged CSS style is applied after a value update.
     */ 
   void changeTimeout( std::chrono::milliseconds & cto /**< [in] the new change timeout in msec */);

   /// Get the change timeout
   /** The change timeout (m_changeTimeout) is the duration for which the 
     * statusChanged CSS style is applied after a value update.
     */
   std::chrono::milliseconds changeTimeout();

   /// Set the edit timeout
   /** The edit timeout (m_editTimeout) is the time after the last keystroke when
     * normal status mode is restored.
     */ 
   void editTimeout( std::chrono::milliseconds & eto /**< [in] the new edit timeout in msec */);

   /// Get the edit timeout
   /** The edit timeout (m_editTimeout) is the time after the last keystroke when
     * normal status mode is restored.
     */
   std::chrono::milliseconds editTimeout();

   /// Set the stale timeout
   /** The stale timeout (m_staleTimeout) is the time after and editing timeout
     * when the edit text is cleared, causing the current value to be the initial 
     * editing text. 
     */ 
   void staleTimeout( std::chrono::milliseconds & sto /**< [in] the new stale timeout in msec */);

   /// Get the stale timeout
   /** The stale timeout (m_staleTimeout) is the time after and editing timeout
     * when the edit text is cleared, causing the current value to be the initial 
     * editing text.
     */
   std::chrono::milliseconds staleTimeout();

   /// Override setText to avoid clobbering editing.
   void setText(const QString & text /**< [in] The new text */);

   /// Set the edit text as if editing with keyboard
   void setEditText( const QString & etext /** [in] the new edit text */);

   /// Stop editing
   /** Calls the editTimerOut slot.
     * 
     */
   void stopEditing();

   /// Adopt the statusChanged CSS style for the duration of m_changeTimeout
   void setTextChanged(const QString & text /**< [in] The new text */);

   

protected:

   /// The focusInEvent triggers editing mode
   /** 
     * \override
     */
   virtual void focusInEvent(QFocusEvent * e);

   /// The focusOutEvent ends editig mode
   /**
     * \override
     */ 
   virtual void focusOutEvent(QFocusEvent * e);

   /// Each keyPressEvent restarts the edit timer
   /**
     * \override
     */
   virtual void keyPressEvent(QKeyEvent * e);

   /// The paintEvent causes changes in the style of this widget based on status/statusChanged/editing mode
   /**
     * \override
     */
   virtual void paintEvent(QPaintEvent * e);

protected slots:

   void changeTimerOut();

   void editTimerOut();

   void staleTimerOut();

signals:
   void changeTimerStart(int);
   void editTimerStart(int);
   void staleTimerStart(int);

};

statusLineEdit::statusLineEdit( QWidget *parent ) : QLineEdit(parent)
{
   m_changeTimer = new QTimer(this);
   connect(m_changeTimer, SIGNAL(timeout()), this, SLOT(changeTimerOut()));
   connect(this, SIGNAL(changeTimerStart(int)), m_changeTimer, SLOT(start(int)));

   m_editTimer = new QTimer(this);
   connect(m_editTimer, SIGNAL(timeout()), this, SLOT(editTimerOut()));
   connect(this, SIGNAL(editTimerStart(int)), m_editTimer, SLOT(start(int)));

   m_staleTimer = new QTimer(this);
   connect(m_staleTimer, SIGNAL(timeout()), this, SLOT(staleTimerOut()));
   connect(this, SIGNAL(staleTimerStart(int)), m_staleTimer, SLOT(start(int)));

   //connect(this, SIGNAL(returnPressed()), this, SLOT(on_returnPressed()));

   QFont qf = font();
   qf.setPixelSize(XW_FONT_SIZE);
   setFont(qf);

}

QString statusLineEdit::currText()
{
   return m_currText;
}

QString statusLineEdit::editText()
{
   return m_editText;
}

void statusLineEdit::readOnly(bool ro)
{
   m_readOnly = ro;

   if(m_readOnly)
   {
      setFocusPolicy(Qt::NoFocus);
   }
   else
   {
      setFocusPolicy(Qt::StrongFocus);
   }

}

bool statusLineEdit::readOnly()
{
   return m_readOnly;
}

void statusLineEdit::highlightChanges(bool hc)
{
   m_highlightChanges = hc;
}

bool statusLineEdit::highlightChanges()
{
   return m_highlightChanges;
}

void statusLineEdit::changeTimeout( std::chrono::milliseconds & cto)
{
   m_changeTimeout = cto;
}

std::chrono::milliseconds statusLineEdit::changeTimeout()
{
   return m_changeTimeout;
}

void statusLineEdit::editTimeout( std::chrono::milliseconds & eto)
{
   m_editTimeout = eto;
}

std::chrono::milliseconds statusLineEdit::editTimeout()
{
   return m_editTimeout;
}

void statusLineEdit::staleTimeout( std::chrono::milliseconds & sto)
{
   m_staleTimeout = sto;
}

std::chrono::milliseconds statusLineEdit::staleTimeout()
{
   return m_staleTimeout;
}

void statusLineEdit::setText(const QString & text)
{
   m_currText = text;
   if(!hasFocus() && m_editing != STARTED) QLineEdit::setText(m_currText);
}

void statusLineEdit::setTextChanged(const QString & text)
{
   m_currText = text;
   if(!hasFocus() && m_editing != STARTED) QLineEdit::setText(m_currText);

   m_editText = "";
   m_valChanged = CHANGED;
}

void statusLineEdit::setEditText( const QString & etext)
{
   m_editTimer->stop();
   m_staleTimer->stop();

   if(m_editing != STARTED) 
   {
      m_currText = text();
   }

   m_editText = etext;
   QLineEdit::setText(m_editText);
   m_editing = STARTED;
   
   emit editTimerStart(m_editTimeout.count());
   update();
}

void statusLineEdit::stopEditing()
{
   editTimerOut();
}

void statusLineEdit::focusInEvent(QFocusEvent * e)
{
   m_editTimer->stop();
   m_staleTimer->stop();
   m_currText = text();
   
   if(m_editText.size() > 0) setText(m_editText);
   m_editing = STARTED;
   
   emit editTimerStart(m_editTimeout.count());
   update();
   QLineEdit::focusInEvent(e);
}

void statusLineEdit::focusOutEvent(QFocusEvent * e)
{
   if(m_editing == STOPPED) //we already stopped, so don't do this.
   {
      e->accept();
      update();
      return;
   }

   m_editText = text();
   setText(m_currText);
   m_editing = STOPPED;
   
   update();
   QLineEdit::focusOutEvent(e);
}

void statusLineEdit::keyPressEvent(QKeyEvent * e)
{
   if(e->key() == Qt::Key_Escape)
   {
      e->accept();
      m_editText = m_currText;
      setText(m_currText);
      m_editing = STOPPED;
      clearFocus();
      update();
      return;
   }
   
   m_editText = text(); //keep updated for return press

   emit editTimerStart(m_editTimeout.count());

   QLineEdit::keyPressEvent(e);  
}

void statusLineEdit::paintEvent(QPaintEvent * e)
{
   if(m_editing == STARTED)
   {
      if(!m_readOnly)
      {
         setProperty("isEditing", true);
         style()->unpolish(this);
      }
   }
   else if(m_editing == STOPPED && m_valChanged == NOTCHANGED)
   {
      setProperty("isEditing", false);
      setProperty("isStatus", true);
      style()->unpolish(this);
   }
   else if(m_valChanged == CHANGED)
   {
      setProperty("isEditing", false);
      setProperty("isStatus", true);
      if(m_highlightChanges)
      {
         setProperty("isStatusChanged", true);
      }
      style()->unpolish(this);
      emit changeTimerStart(m_changeTimeout.count());
      m_editTimer->stop();
      m_staleTimer->stop();
      m_valChanged = 0;
   }
   else if(m_valChanged == CHANGED_TIMEOUT)
   {
      m_changeTimer->stop();
      setProperty("isStatusChanged",false);
      style()->unpolish(this);
      m_valChanged = 0;
   }
   
   QLineEdit::paintEvent(e);
}

void statusLineEdit::changeTimerOut()
{
   m_valChanged = CHANGED_TIMEOUT;
   update();   
}

void statusLineEdit::editTimerOut()
{
   m_editTimer->stop();
   m_editText = text();
   QLineEdit::setText(m_currText);

   emit staleTimerStart(m_staleTimeout.count());
   
   m_editing = STOPPED;
   
   clearFocus(); //This will call onFocusOutEvent

   update();
}

void statusLineEdit::staleTimerOut()
{
   m_staleTimer->stop();
   m_editText = "";
}

} //namespace xqt

#include "moc_statusLineEdit.cpp"

#endif //statusLineEdit_hpp
