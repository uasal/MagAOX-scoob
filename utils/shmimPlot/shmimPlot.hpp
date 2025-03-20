/** \file shmimPlot.hpp
 * \brief The shmimPlot class declaration and definition.
 *
 * \ingroup xrif2hmim_files
 */

#ifndef shmimPlot_hpp
#define shmimPlot_hpp

#include <ImageStreamIO/ImageStruct.h>
#include <ImageStreamIO/ImageStreamIO.h>

#include <xrif/xrif.h>

#include <mx/ioutils/fileUtils.hpp>
#include <mx/improc/eigenCube.hpp>

#include <mx/sys/timeUtils.hpp>
#include <mx/math/plot/gnuPlot.hpp>

#include "../../libMagAOX/libMagAOX.hpp"

/** \defgroup shmimPlot shmimPlot: plot data from a shmim in gnuplot
 * \brief Monitor a shmim and update a gnuplot plot
 *
 * <a href="../handbook/utils/shmimPlot.html">Utility Documentation</a>
 *
 * \ingroup utils
 *
 */

/** \defgroup shmimPlot_files shmimPlot Files
 * \ingroup shmimPlot
 */

bool g_timeToDie = false;

void sigTermHandler( int signum, siginfo_t *siginf, void *ucont )
{
    // Suppress those warnings . . .
    static_cast<void>( signum );
    static_cast<void>( siginf );
    static_cast<void>( ucont );

    std::cerr << "\n"; // clear out the ^C char

    g_timeToDie = true;
}

/// A utility to stream MagaO-X images from xrif compressed archives to an ImageStreamIO stream.
/**
 * \todo finish md doc for shmimPlot
 *
 * \ingroup shmimPlot
 */
class shmimPlot : public mx::app::application
{
  protected:
    /** \name Configurable Parameters
     * @{
     */

    std::string m_shmimName; ///< The name of the shared memory buffer to plot from.

    double m_fps{ 2 }; ///< The rate, in plots per second, at which to plot. Default is 2 Hz.

    bool m_logx{ false };
    bool m_logy{ false };

    float m_x0{ 0 };
    float m_x1{ 0 };
    bool  m_xr_set{ false };

    float m_y0{ 0 };
    float m_y1{ 0 };
    bool  m_yr_set{ false };

    std::vector<int> m_cols;

    std::vector<int> m_rows;
    ///@}

    /** \name Image Data
     * @{
     */

    uint32_t m_width{ 0 };  ///< The width of the image.
    uint32_t m_height{ 0 }; ///< The height of the image.
    uint32_t m_depth{ 0 };  ///< The depth of the cube

    mx::improc::eigenImage<float> m_im;

    uint8_t m_dataType; ///< The ImageStreamIO type code.

    size_t m_typeSize{ 0 }; ///< The size of the type, in bytes.  Result of sizeof.

    IMAGE m_imageStream; ///< The ImageStreamIO shared memory buffer.

    mx::math::gnuPlot m_gp;
    ///@}

  public:
    ~shmimPlot();

    virtual void setupConfig();

    virtual void loadConfig();

    virtual int execute();

    std::vector<std::vector<float>> m_y;

    void doPlot()
    {
        m_gp.clear();


        for(size_t n = 0; n < m_y.size(); ++n)
        {
            m_y[n].resize( m_width );
            for( size_t m = 0; m < m_y[n].size(); ++m )
            {
                m_y[n][m] = m_im( m, m_cols[n] );
            }

            std::string tit = "column " + std::to_string(m_cols[n]);

            m_gp.plot( m_y[n], "w l", tit, tit );
        }
    }
};

inline shmimPlot::~shmimPlot()
{
}

inline void shmimPlot::setupConfig()
{
    config.add( "shmimName",
                "n",
                "shmimName",
                argType::Required,
                "",
                "shmimName",
                false,
                "string",
                "The name of the shared memory buffer to stream to.  Default is \"shmimPlot\"" );
    config.add( "circBuffLength",
                "L",
                "circBuffLength",
                argType::Required,
                "",
                "circBuffLength",
                false,
                "int",
                "The length of the shared memory circular buffer. Default is 1." );

    config.add( "fps",
                "F",
                "fps",
                argType::Required,
                "",
                "fps",
                false,
                "float",
                "The rate, in frames per second, at which to stream images. Default is 10 fps." );

    config.add( "logx", "", "logx", argType::True, "", "logx", false, "bool", "Set the x-axis to log scale" );

    config.add( "logy", "", "logy", argType::True, "", "logy", false, "bool", "Set the y-axis to log scale" );

    config.add( "x0", "", "x0", argType::Required, "", "x0", false, "float", "The x-axis minimum value" );
    config.add( "x1", "", "x1", argType::Required, "", "x1", false, "float", "The x-axis maximum value" );

    config.add( "y0", "", "y0", argType::Required, "", "y0", false, "float", "The y-axis minimum value" );
    config.add( "y1", "", "y1", argType::Required, "", "y1", false, "float", "The y-axis maximum value" );

    config.add( "cols", "c", "cols", argType::Required, "", "cols", false, "vector<int>", "The column(s) to plot" );


}


inline void shmimPlot::loadConfig()
{
    config( m_shmimName, "shmimName" );
    config( m_fps, "fps" );

    if( config.isSet( "logx" ) )
    {
        m_logx = true;
    }

    if( config.isSet( "logy" ) )
    {
        m_logy = true;
    }

    if( config.isSet( "x0" ) && config.isSet( "x1" ) )
    {
        m_xr_set = true;
        config( m_x0, "x0" );
        config( m_x1, "x1" );
    }

    if( config.isSet( "y0" ) && config.isSet( "y1" ) )
    {
        m_yr_set = true;
        config( m_y0, "y0" );
        config( m_y1, "y1" );
    }

    m_cols.push_back(0);
    config(m_cols, "cols");

    if(m_shmimName == "")
    {
        std::cerr << "shmim name not specified with -n\n";
        doHelp = true;
        return;
    }

    m_y.resize(m_cols.size());

    // Setup gnuplot
    if( m_logx )
    {
        m_gp.logx();
    }

    if( m_logy )
    {
        m_gp.logy();
    }

    if( m_xr_set )
    {
        m_gp.xrange( m_x0, m_x1 );
    }

    if( m_yr_set )
    {
        m_gp.yrange( m_y0, m_y1 );
    }


    m_gp.command("set term qt font \"Arial,14\" title \"" +  m_shmimName + "\" noraise");
    m_gp.command("set object 1 rectangle from screen 0,0 to screen 1,1 fillcolor rgb \"#23262a\" behind");
    m_gp.command("set border lc rgb \"#eff0f1\"");
    m_gp.command("set key font \"Arial,14\" textcolor \"#eff0f1\"");
}

inline int shmimPlot::execute()
{


    // Install signal handling
    struct sigaction act;
    sigset_t         set;

    act.sa_sigaction = sigTermHandler;
    act.sa_flags     = SA_SIGINFO;
    sigemptyset( &set );
    act.sa_mask = set;

    errno = 0;
    if( sigaction( SIGTERM, &act, 0 ) < 0 )
    {
        std::cerr << " (" << invokedName << "): error setting SIGTERM handler: " << strerror( errno ) << "\n";
        return -1;
    }

    errno = 0;
    if( sigaction( SIGQUIT, &act, 0 ) < 0 )
    {
        std::cerr << " (" << invokedName << "): error setting SIGQUIT handler: " << strerror( errno ) << "\n";
        return -1;
    }

    errno = 0;
    if( sigaction( SIGINT, &act, 0 ) < 0 )
    {
        std::cerr << " (" << invokedName << "): error setting SIGINT handler: " << strerror( errno ) << "\n";
        return -1;
    }



    bool  opened = false;
    bool  restart;
    ino_t inode           = 0; ///< The inode of the image stream file
    int   semaphoreNumber = 9; ///< The image structure semaphore index.

    float ( *pixget )( void *, size_t ){ nullptr }; ///< Pointer to a function to extract the image data as float

    while( !g_timeToDie )
    {
        /* Initialize ImageStreamIO
         */
        opened  = false;
        restart = false; // Set this up front, since we're about to restart.

        int logged = 0;
        while( !opened && !g_timeToDie && !restart )
        {
            // b/c ImageStreamIO prints every single time, and latest version don't support stopping it yet, and that
            // isn't thread-safe-able anyway we do our own checks.  This is the same code in ImageStreamIO_openIm...
            int  SM_fd;
            char SM_fname[200];
            ImageStreamIO_filename( SM_fname, sizeof( SM_fname ), m_shmimName.c_str() );
            SM_fd = open( SM_fname, O_RDWR );
            if( SM_fd == -1 )
            {
                if( !logged )
                {
                    std::cerr << "ImageStream " + m_shmimName + " not found (yet).  Retrying . . .\n";
                }
                logged = 1;
                sleep( 1 ); // be patient
                continue;
            }

            // Found and opened,  close it and then use ImageStreamIO
            logged = 0;
            close( SM_fd );

            if( ImageStreamIO_openIm( &m_imageStream, m_shmimName.c_str() ) == 0 )
            {
                if( m_imageStream.md[0].sem <=
                    semaphoreNumber ) ///<\todo this isn't right--> isn't there a define in cacao to use?
                {
                    ImageStreamIO_closeIm( &m_imageStream );
                    mx::sys::sleep( 1 ); // We just need to wait for the server process to finish startup.
                }
                else
                {
                    opened = true;
                    char SM_fname[200];
                    ImageStreamIO_filename( SM_fname, sizeof( SM_fname ), m_shmimName.c_str() );

                    struct stat buffer;
                    int         rv = stat( SM_fname, &buffer );

                    if( rv != 0 )
                    {
                        std::cerr << "Could not get inode for " + m_shmimName +
                                         ". Source process will need to be restarted.\n";
                        ImageStreamIO_closeIm( &m_imageStream );
                        return -1;
                    }
                    inode = buffer.st_ino;
                }
            }
            else
            {
                mx::sys::sleep( 1 ); // be patient
            }
        }

        if( restart )
        {
            continue; // this is kinda dumb.  we just go around on restart, so why test in the while loop at all?
        }

        if( g_timeToDie )
        {
            if( !opened )
            {
                return 0;
            }

            ImageStreamIO_closeIm( &m_imageStream );
            return 0;
        }

        semaphoreNumber =
            ImageStreamIO_getsemwaitindex( &m_imageStream, semaphoreNumber ); // ask for semaphore we had before

        if( semaphoreNumber < 0 )
        {
            std::cerr << "No valid semaphore found for " + m_shmimName +
                             ". Source process will need to be restarted.\n";
            return -1;
        }

        ImageStreamIO_semflush( &m_imageStream, semaphoreNumber );

        sem_t *sem = m_imageStream.semptr[semaphoreNumber]; ///< The semaphore to monitor for new image data

        m_dataType = m_imageStream.md[0].datatype;
        m_typeSize = ImageStreamIO_typesize( m_dataType );
        m_width    = m_imageStream.md[0].size[0];
        int dim    = 1;
        if( m_imageStream.md[0].naxis > 1 )
        {
            m_height = m_imageStream.md[0].size[1];

            if( m_imageStream.md[0].naxis > 2 )
            {
                dim     = 3;
                m_depth = m_imageStream.md[0].size[2];
            }
            else
            {
                dim     = 2;
                m_depth = 1;
            }
        }
        else
        {
            m_height = 1;
            m_depth  = 1;
        }

        uint8_t  atype;
        size_t   snx, sny, snz;
        uint64_t curr_image; // The current cnt1 index

        if( m_imageStream.md[0].size[2] > 0 ) ///\todo change to naxis?
        {
            curr_image = m_imageStream.md[0].cnt1;
        }
        else
        {
            curr_image = 0;
        }

        atype = m_imageStream.md[0].datatype;
        snx   = m_imageStream.md[0].size[0];

        if( dim == 2 )
        {
            sny = m_imageStream.md[0].size[1];
            snz = 1;
        }
        else if( dim == 3 )
        {
            sny = m_imageStream.md[0].size[1];
            snz = m_imageStream.md[0].size[2];
        }
        else
        {
            sny = 1;
            snz = 1;
        }

        if( atype != m_dataType || snx != m_width || sny != m_height || snz != m_depth )
        {
            continue; // exit the nearest while loop and get the new image setup.
        }

        m_im.resize( m_width, m_height );
        pixget = getPixPointer<float>( m_dataType );

        char *raw = reinterpret_cast<char *>( m_imageStream.array.raw ) + curr_image * m_width * m_height * m_typeSize;

        for( size_t idx = 0; idx < m_width * m_height; ++idx )
        {
            m_im( idx ) = pixget( raw, idx );
        }

        for(size_t n = 0; n < m_y.size(); ++n)
        {
            m_y[n].resize( m_width );
        }

        doPlot();

        // This is the main image grabbing loop.
        while( !g_timeToDie && !restart )
        {
            timespec ts;

            if( clock_gettime( CLOCK_REALTIME, &ts ) < 0 )
            {
                std::cerr << "error from clock_gettime\n";
                return -1;
            }

            ts.tv_sec += 1;

            if( sem_timedwait( sem, &ts ) == 0 )
            {
                if( m_imageStream.md[0].size[2] > 0 ) ///\todo change to naxis?
                {
                    curr_image = m_imageStream.md[0].cnt1;
                }
                else
                {
                    curr_image = 0;
                }

                atype = m_imageStream.md[0].datatype;
                snx   = m_imageStream.md[0].size[0];

                if( dim == 2 )
                {
                    sny = m_imageStream.md[0].size[1];
                    snz = 1;
                }
                else if( dim == 3 )
                {
                    sny = m_imageStream.md[0].size[1];
                    snz = m_imageStream.md[0].size[2];
                }
                else
                {
                    sny = 1;
                    snz = 1;
                }

                if( atype != m_dataType || snx != m_width || sny != m_height || snz != m_depth )
                {
                    break; // exit the nearest while loop and get the new image setup.
                }

                if( g_timeToDie || restart )
                {
                    break; // Check for exit signals
                }

                char *raw =
                    reinterpret_cast<char *>( m_imageStream.array.raw ) + curr_image * m_width * m_height * m_typeSize;
                for( size_t idx = 0; idx < m_width * m_height; ++idx )
                {
                    m_im( idx ) = pixget( raw, idx );
                }

                doPlot();
            }
            else
            {
                if( m_imageStream.md[0].sem <= 0 )
                    break; // Indicates that the server has cleaned up.

                // Check for why we timed out
                if( errno == EINTR )
                    break; // This indicates signal interrupted us, time to restart or shutdown, loop will exit normally
                           // if flags set.

                // ETIMEDOUT means we should check for deletion, and then wait more.
                // Otherwise, report an error.
                if( errno != ETIMEDOUT )
                {
                    std::cerr << "error from sem_timedwait\n";
                    break;
                }

                // Check if the file has disappeared.
                int  SM_fd;
                char SM_fname[200];
                ImageStreamIO_filename( SM_fname, sizeof( SM_fname ), m_shmimName.c_str() );
                SM_fd = open( SM_fname, O_RDWR );
                if( SM_fd == -1 )
                {
                    restart = true;
                }
                close( SM_fd );

                // Check if the inode changed
                struct stat buffer;
                int         rv = stat( SM_fname, &buffer );
                if( rv != 0 )
                {
                    restart = true;
                }

                if( buffer.st_ino != inode )
                {
                    restart = true;
                }
            }
        }

        // opened == true if we can get to this
        if( semaphoreNumber >= 0 )
            m_imageStream.semReadPID[semaphoreNumber] = 0; // release semaphore
        ImageStreamIO_closeIm( &m_imageStream );
        opened = false;

    } // outer loop, will exit if m_shutdown==true

    m_gp.command("set term qt close");

    if( opened )
    {
        ImageStreamIO_closeIm( &m_imageStream );
    }

    return 0;
}

#endif // shmimPlot_hpp
