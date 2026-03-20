#ifndef warnings_hpp
#define warnings_hpp

#include <rtimv/rtimvInterfaces.hpp>
#include <rtimv/StretchBox.hpp>

#include <QObject>
#include <QtPlugin>

#include <iostream>

class warnings : public rtimvOverlayInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA( IID "rtimv.overlayInterface/1.4" )
    Q_INTERFACES( rtimvOverlayInterface )

  protected:
    rtimvOverlayAccess m_roa;

    bool m_enabled{ false };

    bool m_enableable{ false };

    std::string m_deviceName;

    std::vector<std::string> m_cautionKeys;

    std::vector<std::string> m_warningKeys;

    std::vector<std::string> m_alertKeys;

    char m_blob[512]; ///< Memory for copying rtimvDictionary blobs

  public:
    warnings();

    virtual ~warnings();

    virtual int attachOverlay( rtimvOverlayAccess &, mx::app::appConfigurator &config );

    virtual int updateOverlay();

    virtual void keyPressEvent( QKeyEvent *ke );

    virtual bool hasTextOverlay();

    virtual char textOverlayKey();

    virtual std::string textOverlayTitle();

    virtual std::string textOverlayText();

    virtual bool overlayEnabled();

    virtual void enableOverlay();

    virtual void disableOverlay();

  public:
    virtual std::vector<std::string> info();

  protected:
    bool keyOn( const std::string &key );

    bool anyOn( const std::vector<std::string> &keys, const std::string &prefix );

    void appendActive( std::string                    &text,
                       const std::string              &heading,
                       const std::vector<std::string> &keys,
                       const std::string              &prefix );

  signals:

    void warningLevel( rtimv::warningLevel lvl );
};

#endif // warnings_hpp
