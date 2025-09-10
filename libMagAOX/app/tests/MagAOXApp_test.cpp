// #define CATCH_CONFIG_MAIN
#include "../../../tests/catch2/catch.hpp"

#include <mx/sys/timeUtils.hpp>

#include "../MagAOXApp.hpp"

namespace MagAOXApp_tests
{

struct MagAOXApp_test : public MagAOX::app::MagAOXApp<true>
{
    MagAOXApp_test() : MagAOXApp( "sha1", false )
    {
    }

    virtual int appStartup()
    {
        return 0;
    }
    virtual int appLogic()
    {
        return 0;
    }
    virtual int appShutdown()
    {
        return 0;
    }

    std::string MagAOXPath()
    {
        return MagAOX::app::MagAOXApp<true>::MagAOXPath;
    }

    std::string configPathGlobal()
    {
        return MagAOX::app::MagAOXApp<true>::m_configPathGlobal;
    }

    std::string calibDir()
    {
        return MagAOX::app::MagAOXApp<true>::m_calibDir;
    }

    std::string sysPath()
    {
        return MagAOX::app::MagAOXApp<true>::sysPath;
    }

    std::string secretsPath()
    {
        return MagAOX::app::MagAOXApp<true>::secretsPath;
    }

    std::string cpusetPath()
    {
        return MagAOX::app::MagAOXApp<true>::m_cpusetPath;
    }

    std::string configPathUser()
    {
        return MagAOX::app::MagAOXApp<true>::m_configPathUser;
    }

    std::string configPathLocal()
    {
        return MagAOX::app::MagAOXApp<true>::m_configPathLocal;
    }

    std::string &invokedName()
    {
        return MagAOX::app::MagAOXApp<true>::invokedName;
    }

    bool &doHelp()
    {
        return MagAOX::app::MagAOXApp<true>::doHelp;
    }

    std::string configName()
    {
        return MagAOX::app::MagAOXApp<true>::configName();
    }

    void configName( const std::string &cn )
    {
        m_configName = cn;

        m_indiDriver = new MagAOX::app::indiDriver<MagAOX::app::MagAOXApp<true>>( this, m_configName, "0", "0" );
    }

    int called_back{ 0 };
};

int callback( void *app, const pcf::IndiProperty &ipRecv )
{
    static_cast<void>( ipRecv ); // be unused

    MagAOXApp_test *appt = static_cast<MagAOXApp_test *>( app );

    appt->called_back = 1;

    return 0;
}

SCENARIO( "MagAOXApp INDI NewProperty", "[app::MagAOXApp]" )
{
    GIVEN( "a new property request" )
    {
        WHEN( "a wrong device name" )
        {
            MagAOXApp_test app;

            app.configName( "test" );

            REQUIRE( app.configName() == "test" );

            pcf::IndiProperty prop;
            app.registerIndiPropertyNew( prop,
                                         "nprop",
                                         pcf::IndiProperty::Number,
                                         pcf::IndiProperty::ReadWrite,
                                         pcf::IndiProperty::Idle,
                                         callback );

            pcf::IndiProperty nprop;

            // First test the right device name
            nprop.setDevice( "test" );
            nprop.setName( "nprop" );

            app.handleNewProperty( nprop );

            REQUIRE( app.called_back == 1 );

            app.called_back = 0;

            // Now test the wrong device name
            nprop.setDevice( "wrong" );

            app.handleNewProperty( nprop );

            REQUIRE( app.called_back == 0 );
        }
    }
}

TEST_CASE( "Setting defaults", "[app::MagAOXApp]" )
{
    SECTION( "using default paths, configname is invoked name" )
    {
        std::vector<std::string> argvstr( { "./execname" } );

        std::vector<const char *> argv( argvstr.size() + 1, NULL );
        for( size_t index = 0; index < argvstr.size(); ++index )
        {
            argv[index] = argvstr[index].c_str();
        }

        MagAOXApp_test app;

        app.invokedName() = argv[0];
        app.setDefaults( argv.size() - 1, const_cast<char **>( argv.data() ) );

        REQUIRE( app.MagAOXPath() == MAGAOX_path );
        REQUIRE( app.configDir() == app.MagAOXPath() + '/' + MAGAOX_configRelPath );
        REQUIRE( app.configPathGlobal() == app.configDir() + "/magaox.conf" );
        REQUIRE( app.calibDir() == app.MagAOXPath() + '/' + MAGAOX_calibRelPath );
        REQUIRE( app.sysPath() == app.MagAOXPath() + '/' + MAGAOX_sysRelPath );
        REQUIRE( app.secretsPath() == app.MagAOXPath() + '/' + MAGAOX_secretsRelPath );
        REQUIRE( app.cpusetPath() == MAGAOX_cpusetPath );
        REQUIRE( app.configPathUser() == "" );
        REQUIRE( app.configName() == "execname" );
        REQUIRE( app.configPathLocal() == app.configDir() + "/execname.conf" );

        REQUIRE( app.doHelp() == true );
    }

    SECTION( "using default paths, with config-ed name" )
    {
        std::vector<std::string> argvstr( { "./execname", "-n", "testapp" } );

        std::vector<const char *> argv( argvstr.size() + 1, NULL );
        for( size_t index = 0; index < argvstr.size(); ++index )
        {
            argv[index] = argvstr[index].c_str();
        }

        MagAOXApp_test app;
        app.invokedName() = argv[0];

        app.setDefaults( argv.size() - 1, const_cast<char **>( argv.data() ) );

        REQUIRE( app.MagAOXPath() == MAGAOX_path );
        REQUIRE( app.configDir() == app.MagAOXPath() + '/' + MAGAOX_configRelPath );
        REQUIRE( app.configPathGlobal() == app.configDir() + "/magaox.conf" );
        REQUIRE( app.calibDir() == app.MagAOXPath() + '/' + MAGAOX_calibRelPath );
        REQUIRE( app.sysPath() == app.MagAOXPath() + '/' + MAGAOX_sysRelPath );
        REQUIRE( app.secretsPath() == app.MagAOXPath() + '/' + MAGAOX_secretsRelPath );
        REQUIRE( app.cpusetPath() == MAGAOX_cpusetPath );
        REQUIRE( app.configPathUser() == "" );
        REQUIRE( app.configName() == "testapp" );
        REQUIRE( app.configPathLocal() == app.configDir() + "/testapp.conf" );
        REQUIRE( app.doHelp() == false );
    }

    //Something goes wrong here, third time is the charm.
    // Hangs on config.parseCommandLine
    /*SECTION( "using environment paths, with config-ed name" )
    {
        std::vector<const char *> argv;
        std::vector<std::string>  argvstr( { "./execname", "-n", "testapp" } );

        argv.resize( argvstr.size() + 1, NULL );
        for( size_t index = 0; index < argvstr.size(); ++index )
        {
            argv[index] = argvstr[index].c_str();
        }

        char ppath[4096];
        snprintf( ppath, sizeof( ppath ), "%s=/tmp/MagAOXApp_test", MAGAOX_env_path );
        putenv( ppath );

        MagAOXApp_test app;

        app.invokedName() = argv[0];

        app.setDefaults( argv.size() - 1, const_cast<char **>( argv.data() ) );

        REQUIRE( app.MagAOXPath() == "/tmp/MagAOXApp_test" );
        REQUIRE( app.configDir() == app.MagAOXPath() + '/' + MAGAOX_configRelPath );
        REQUIRE( app.configPathGlobal() == app.configDir() + "/magaox.conf" );
        REQUIRE( app.calibDir() == app.MagAOXPath() + '/' + MAGAOX_calibRelPath );
        REQUIRE( app.sysPath() == app.MagAOXPath() + '/' + MAGAOX_sysRelPath );
        REQUIRE( app.secretsPath() == app.MagAOXPath() + '/' + MAGAOX_secretsRelPath );
        REQUIRE( app.cpusetPath() == MAGAOX_cpusetPath );
        REQUIRE( app.configPathUser() == "" );
        REQUIRE( app.configName() == "testapp" );
        REQUIRE( app.configPathLocal() == app.configDir() + "/testapp.conf" );
        REQUIRE( app.doHelp() == false );
    }*/
}

} // namespace MagAOXApp_tests
