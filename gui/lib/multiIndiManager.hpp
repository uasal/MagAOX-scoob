#ifndef multiIndiManager_hpp
#define multiIndiManager_hpp

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

#include <QObject>
#include <QTimer>

#include <unistd.h>

#include "multiIndiPublisher.hpp"

inline void _dispatchOnDisconnect( multiIndiSubscriber * sub )
{
   if(auto * obj = dynamic_cast<QObject *>(sub))
   {
      QTimer::singleShot(0, obj, [sub]() { sub->onDisconnect(); });
      return;
   }

   sub->onDisconnect();
}

/// Class to manage an INDI publisher and multiple INDI subscribers
/** Primary purpose of this class is to detect lack/loss of connection and
  * reconnect when able, then re-initialize the subscriptions.
  *
  *
  */
class multiIndiManager : public multiIndiSubscriber
{

protected:
   std::string m_clientName;  ///< Name used for the INDI client
   std::string m_hostAddress; ///< Address of the indiserver host to connect to
   int m_hostPort {0};        ///< Port on the host for indiserver

   //std::vector<multiIndiSubscriber *> m_subscribers; ///< Pointers to the subscribers themselves

   multiIndiPublisher * m_publisher {nullptr}; ///< The publisher, which is the INDI client which manages the distrubtion of properties to subscribers.
   std::mutex m_mutex;

   std::thread m_monThread;

   std::atomic_bool m_shutdown {false};

public:
   /// Default c'tor
   /*
    */
   multiIndiManager();

   /// Constructor which sets up and initiates the connection
   /*
    */
   multiIndiManager( const std::string & clientName,  ///< [in]
                     const std::string & hostAddress, ///< [in]
                     const int hostPort               ///< [in]
                   );

   /// Destructor
   /* Disconnects and cleans up the client.
    */
   ~multiIndiManager();

   /// Get the
   /**
     * \returns the current value of
     */
   std::string clientName();

   /// Set the
   /** After setting this, you will need to call activate(true) to reset the client.
     */
   void clientName( const std::string & cn /**< [in] the new*/);

   /// Get the
   /**
     * \returns the current value of
     */
   std::string hostAddress();

   /// Set the
   /** After setting this, you will need to call activate(true) to reset the client.
     */
   void hostName( const std::string & hn /**< [in] the new*/);

   /// Get the
   /**
     * \returns the current value of
     */
   int hostPort();

   /// Set the
   /** After setting this, you will need to call activate(true) to reset the client.
     */
   void hostPort( int hp  /**< [in] the new*/);

   /// Add a subscriber.
   /** If connected, this immediately calls the subscribers subscribe member function.
     */
   virtual int addSubscriber( multiIndiSubscriber * sub /**< [in] the subscriber to add*/);
   virtual void unsubscribe( multiIndiSubscriber * sub );

   virtual void sendNewProperty( const pcf::IndiProperty &ipSend)
   {
      std::lock_guard<std::mutex> lock(m_mutex);
      if(m_publisher) m_publisher->sendNewProperty(ipSend);
   }

   virtual void sendGetProperties(const pcf::IndiProperty &ipSend)
   {
      std::lock_guard<std::mutex> lock(m_mutex);
      if(m_publisher) m_publisher->sendGetProperties(ipSend);
   }

   ///
   /*
    */
   void activate(bool force = false /**< [in] if true, then this will force a reconnection */);

public: //todo: make a protected static member

   void connectClient();
};

inline
multiIndiManager::multiIndiManager()
{
}

inline
multiIndiManager::multiIndiManager( const std::string & clientName,
                                    const std::string & hostAddress,
                                    const int hostPort
                                  ) : m_clientName {clientName}, m_hostAddress{hostAddress}, m_hostPort{hostPort}
{
}

inline
multiIndiManager::~multiIndiManager()
{
   m_shutdown.store(true, std::memory_order_relaxed);

   if(m_monThread.joinable())
   {
      try
      {
         m_monThread.join(); //this will throw if it was already joined
      }
      catch(...)
      {
      }
   }
}

inline
void _connectStart( multiIndiManager * mim )
{
   mim->connectClient();
}

inline
void multiIndiManager::activate(bool force)
{
   if(force)
   {
      m_shutdown.store(true, std::memory_order_relaxed);

      if(m_monThread.joinable())
      {
         try
         {
            m_monThread.join(); //this will throw if it was already joined
         }
         catch(...)
         {
          }
      }
      m_shutdown.store(false, std::memory_order_relaxed);
   }

   if(m_monThread.joinable()) return; //Already running

   try
   {
      m_monThread  = std::thread( _connectStart, this);
   }
   catch( const std::exception & e )
   {
      std::cerr << "Exception while activating INDI connection thread: " << e.what() << "\n";
   }
   catch( ... )
   {
      std::cerr << "Unknown exception while activating INDI connection thread.\n";
   }
}



inline
int multiIndiManager::addSubscriber( multiIndiSubscriber * sub )
{
   std::lock_guard<std::mutex> lock(m_mutex);
   subscribers.insert(sub);

   if(auto * obj = dynamic_cast<QObject *>(sub))
   {
      QObject::connect(obj, &QObject::destroyed, [this, sub]() { this->unsubscribe(sub); });
   }

   if(m_publisher != nullptr)
   {
      m_publisher->addSubscriber(sub);
   }

   return 0;
}

inline
void multiIndiManager::unsubscribe( multiIndiSubscriber * sub )
{
   std::lock_guard<std::mutex> lock(m_mutex);
   subscribers.erase(sub);
   if(m_publisher != nullptr)
   {
      m_publisher->unsubscribe(sub);
   }
}

inline
void multiIndiManager::connectClient()
{
   while( !m_shutdown.load(std::memory_order_relaxed) )
   {
      multiIndiPublisher * pub {nullptr};
      std::vector<multiIndiSubscriber *> subs;
      bool doDisconnect {false};
      bool doConnect {false};

      {
         std::lock_guard<std::mutex> lock(m_mutex);
         if(m_publisher != nullptr) //Check to see if we're still connected
         {
            if(m_publisher->getQuitProcess() || m_publisher->disconnect() || m_shutdown.load(std::memory_order_relaxed))
            {
               pub = m_publisher;
               m_publisher = nullptr;
               subs.reserve(subscribers.size());
               for(auto it = subscribers.begin(); it != subscribers.end(); ++it) subs.push_back(*it);
               doDisconnect = true;
            }
         }
         else
         {
            doConnect = true;
         }
      }

      if(doDisconnect)
      {
         pub->quitProcess();
         pub->deactivate();

         for(auto * sub : subs)
         {
            _dispatchOnDisconnect(sub);
            pub->unsubscribe(sub);
         }

         delete pub;
      }

      if(doConnect) //try to connect
      {
         multiIndiPublisher * candidate {nullptr};
         try
         {
            candidate = new multiIndiPublisher(m_clientName, m_hostAddress, m_hostPort);
         }
         catch(...)
         {
            sleep(1);
            continue;
         }

         candidate->activate();

         sleep(5);

         //Check connection
         if(candidate->getQuitProcess() || m_shutdown.load(std::memory_order_relaxed)) //not connected
         {
            candidate->deactivate();
            delete candidate;

         }
         else //connected
         {
            {
               std::lock_guard<std::mutex> lock(m_mutex);
               m_publisher = candidate;
               subs.reserve(subscribers.size());
               for(auto it = subscribers.begin(); it != subscribers.end(); ++it) subs.push_back(*it);
            }

            for(auto * sub : subs)
            {
               candidate->addSubscriber(sub);
            }
            candidate->onConnect();
         }
      }

      sleep(1);
   }

   multiIndiPublisher * pub {nullptr};
   std::vector<multiIndiSubscriber *> subs;
   {
      std::lock_guard<std::mutex> lock(m_mutex);
      if(m_publisher != nullptr) //Before exiting, disconnect.
      {
         pub = m_publisher;
         m_publisher = nullptr;
         subs.reserve(subscribers.size());
         for(auto it = subscribers.begin(); it != subscribers.end(); ++it) subs.push_back(*it);
      }
   }

   if(pub != nullptr)
   {
      pub->quitProcess();
      pub->deactivate();

      for(auto * sub : subs)
      {
         _dispatchOnDisconnect(sub);
         pub->unsubscribe(sub);
      }

      delete pub;
   }
}


#endif  //multiIndiManager_hpp
