#pragma once

/// CPython embedding for optical_bridge.py (esc_llowfsc_sim lives in Python).
/// All methods must be called from the same thread that called initialize().

#ifndef LLOWFSC_OPTICAL_MODEL_EMBED_HPP
#define LLOWFSC_OPTICAL_MODEL_EMBED_HPP

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define PY_ARRAY_UNIQUE_SYMBOL LLOWFSCSIMCPP_ARRAY_API
#include <numpy/arrayobject.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace llowfscsim {

inline std::string pyErrString()
{
    if( !PyErr_Occurred() )
        return {};
    PyObject *type = nullptr, *value = nullptr, *tb = nullptr;
    PyErr_Fetch( &type, &value, &tb );
    PyErr_NormalizeException( &type, &value, &tb );
    std::string msg = "Python error";
    if( value )
    {
        PyObject *s = PyObject_Str( value );
        if( s )
        {
            const char *c = PyUnicode_AsUTF8( s );
            if( c )
                msg = c;
            Py_DECREF( s );
        }
    }
    Py_XDECREF( type );
    Py_XDECREF( value );
    Py_XDECREF( tb );
    return msg;
}

struct SnapRequest
{
    const double *dm = nullptr; ///< nact*nact meters, row-major C; null → use dm_flat
    int nact = 0;
    double fsm_x_nm = 0;
    double fsm_y_nm = 0;
    double exp = 0.01;
    double gain = 120;
    double blacklevel = 0;
    int bitdepth = 16;
    bool shutter_closed = false;
    bool use_vortex = true;
    double vmag = 0;
    int roi_w = 0;
    int roi_h = 0;
    bool camlo = false;
    const double *opd = nullptr; ///< 10 zernike coeffs or null
};

struct ModelConfig
{
    std::string model_pack{ "1k-256" };
    std::string cuda_device{ "0" };
    double wavelength_c{ 633e-9 };
    int ncamsci{ 512 };
    int nwaves{ 51 };
    double bw{ 0.02 };
    double dm_astig_rms{ 0.0 };
    bool compute_flat{ true };
    bool plot_flat{ false };
    double dark_current{ 0.025 };
    int nbits{ 14 };
    double qe{ 0.75 };
    double default_exp_time{ 0.01 };
    double default_gain{ 120.0 };
    double default_blacklevel{ 10.0 };
    int default_bit_depth{ 16 };
    bool usevortex{ true };
};

class OpticalModel
{
  public:
    std::string lastError;
    std::string lastInfo;

    ~OpticalModel()
    {
        shutdown();
    }

    bool initialize( const std::string &pythonPrefix, const std::string &bridgeDir,
                     const std::string &cudaDevice )
    {
        if( m_ready )
            return true;

        if( !cudaDevice.empty() )
            setenv( "CUDA_VISIBLE_DEVICES", cudaDevice.c_str(), 1 );
        // Do not set PYTHONNOUSERSITE: esc_llowfsc_sim currently needs tomlkit,
        // which may live in the MagAO-X user's ~/.local. Prefer installing
        // tomlkit into the conda env for setuid / other-user launches.
        if( !pythonPrefix.empty() )
            setenv( "PYTHONHOME", pythonPrefix.c_str(), 1 );

#if PY_VERSION_HEX >= 0x03080000
        PyStatus status;
        PyConfig config;
        PyConfig_InitPythonConfig( &config );
        if( !pythonPrefix.empty() )
        {
            status = PyConfig_SetBytesString( &config, &config.home,
                                              pythonPrefix.c_str() );
            if( PyStatus_Exception( status ) )
            {
                lastError = "PyConfig_SetBytesString(home) failed";
                PyConfig_Clear( &config );
                return false;
            }
        }
        status = Py_InitializeFromConfig( &config );
        PyConfig_Clear( &config );
        if( PyStatus_Exception( status ) )
        {
            lastError = "Py_InitializeFromConfig failed";
            return false;
        }
#else
        if( !pythonPrefix.empty() )
        {
            static std::wstring home;
            PyObject *s = PyUnicode_FromString( pythonPrefix.c_str() );
            if( s )
            {
                home = std::wstring( PyUnicode_AsWideCharString( s, nullptr ) );
                Py_DECREF( s );
                Py_SetPythonHome( const_cast<wchar_t *>( home.c_str() ) );
            }
        }
        Py_Initialize();
#endif

        // import_array() is a statement-macro that `return`s on failure; call the
        // underlying function so this bool method does not `return -1`.
        if( _import_array() < 0 )
        {
            lastError = "numpy C API import_array failed: " + pyErrString();
            return false;
        }

        PyObject *sysPath = PySys_GetObject( "path" );
        if( sysPath && !bridgeDir.empty() )
        {
            PyObject *p = PyUnicode_FromString( bridgeDir.c_str() );
            if( p )
            {
                PyList_Insert( sysPath, 0, p );
                Py_DECREF( p );
            }
        }

        m_mod = PyImport_ImportModule( "optical_bridge" );
        if( !m_mod )
        {
            lastError = "import optical_bridge failed: " + pyErrString();
            return false;
        }
        m_ready = true;
        return true;
    }

    bool create( const ModelConfig &cfg, int &ncamsciOut, int &nactOut )
    {
        lastError.clear();
        if( !m_mod )
        {
            lastError = "Python not initialized";
            return false;
        }
        PyObject *d = PyDict_New();
        auto setS = [&]( const char *k, const std::string &v ) {
            PyObject *o = PyUnicode_FromString( v.c_str() );
            PyDict_SetItemString( d, k, o );
            Py_DECREF( o );
        };
        auto setF = [&]( const char *k, double v ) {
            PyObject *o = PyFloat_FromDouble( v );
            PyDict_SetItemString( d, k, o );
            Py_DECREF( o );
        };
        auto setI = [&]( const char *k, long v ) {
            PyObject *o = PyLong_FromLong( v );
            PyDict_SetItemString( d, k, o );
            Py_DECREF( o );
        };
        auto setB = [&]( const char *k, bool v ) {
            PyObject *o = PyBool_FromLong( v ? 1 : 0 );
            PyDict_SetItemString( d, k, o );
            Py_DECREF( o );
        };
        setS( "model_pack", cfg.model_pack );
        setS( "cuda_device", cfg.cuda_device );
        setF( "wavelength_c", cfg.wavelength_c );
        setI( "ncamsci", cfg.ncamsci );
        setI( "nwaves", cfg.nwaves );
        setF( "bw", cfg.bw );
        setF( "dm_astig_rms", cfg.dm_astig_rms );
        setB( "compute_flat", cfg.compute_flat );
        setB( "plot_flat", cfg.plot_flat );
        setF( "dark_current", cfg.dark_current );
        setI( "nbits", cfg.nbits );
        setF( "qe", cfg.qe );
        setF( "default_exp_time", cfg.default_exp_time );
        setF( "default_gain", cfg.default_gain );
        setF( "default_blacklevel", cfg.default_blacklevel );
        setI( "default_bit_depth", cfg.default_bit_depth );
        setB( "usevortex", cfg.usevortex );

        PyObject *fn = PyObject_GetAttrString( m_mod, "create" );
        PyObject *res = PyObject_CallFunctionObjArgs( fn, d, nullptr );
        Py_DECREF( fn );
        Py_DECREF( d );
        if( !res )
        {
            lastError = "optical_bridge.create failed: " + pyErrString();
            return false;
        }
        PyObject *nc = PyDict_GetItemString( res, "ncamsci" );
        PyObject *na = PyDict_GetItemString( res, "nact" );
        ncamsciOut = nc ? static_cast<int>( PyLong_AsLong( nc ) ) : 0;
        nactOut = na ? static_cast<int>( PyLong_AsLong( na ) ) : 0;
        lastInfo.clear();
        auto takeS = [&]( const char *k ) {
            PyObject *o = PyDict_GetItemString( res, k );
            if( !o || !PyUnicode_Check( o ) )
                return;
            const char *c = PyUnicode_AsUTF8( o );
            if( !c )
                return;
            if( !lastInfo.empty() )
                lastInfo += " ";
            lastInfo += std::string( k ) + "=" + c;
        };
        takeS( "xp" );
        takeS( "esc" );
        takeS( "sys_prefix" );
        takeS( "fast" );
        Py_DECREF( res );
        return ncamsciOut > 0 && nactOut > 0;
    }

    bool setDmFlat( const double *flat_m, int nact )
    {
        if( !m_mod || !flat_m || nact <= 0 )
            return false;
        npy_intp dims[2] = { nact, nact };
        PyObject *arr = PyArray_SimpleNew( 2, dims, NPY_DOUBLE );
        if( !arr )
        {
            lastError = "PyArray_SimpleNew dm_flat failed";
            return false;
        }
        std::memcpy( PyArray_DATA( reinterpret_cast<PyArrayObject *>( arr ) ),
                     flat_m, static_cast<size_t>( nact ) * static_cast<size_t>( nact ) *
                                 sizeof( double ) );
        PyObject *fn = PyObject_GetAttrString( m_mod, "set_dm_flat" );
        PyObject *res = PyObject_CallFunctionObjArgs( fn, arr, nullptr );
        Py_DECREF( fn );
        Py_DECREF( arr );
        if( !res )
        {
            lastError = "set_dm_flat failed: " + pyErrString();
            return false;
        }
        Py_DECREF( res );
        return true;
    }

    bool snap( const SnapRequest &req, std::vector<float> &out, uint32_t &w, uint32_t &h )
    {
        lastError.clear();
        if( !m_mod )
        {
            lastError = "Python not initialized";
            return false;
        }

        PyObject *dmObj = Py_None;
        Py_INCREF( Py_None );
        if( req.dm && req.nact > 0 )
        {
            Py_DECREF( dmObj );
            npy_intp dims[2] = { req.nact, req.nact };
            dmObj = PyArray_SimpleNew( 2, dims, NPY_DOUBLE );
            if( !dmObj )
            {
                lastError = "PyArray_SimpleNew dm failed";
                return false;
            }
            std::memcpy( PyArray_DATA( reinterpret_cast<PyArrayObject *>( dmObj ) ),
                         req.dm,
                         static_cast<size_t>( req.nact ) * static_cast<size_t>( req.nact ) *
                             sizeof( double ) );
        }

        PyObject *opdObj = Py_None;
        Py_INCREF( Py_None );
        if( req.opd )
        {
            Py_DECREF( opdObj );
            npy_intp dims[1] = { 10 };
            opdObj = PyArray_SimpleNew( 1, dims, NPY_DOUBLE );
            if( opdObj )
                std::memcpy( PyArray_DATA( reinterpret_cast<PyArrayObject *>( opdObj ) ),
                             req.opd, 10 * sizeof( double ) );
        }

        PyObject *kwargs = PyDict_New();
        PyDict_SetItemString( kwargs, "dm", dmObj );
        PyDict_SetItemString( kwargs, "opd", opdObj );
        PyObject *fn = PyObject_GetAttrString( m_mod, "snap" );
        PyObject *args = PyTuple_New( 0 );
        // Build kwargs with scalars via Py_BuildValue pieces
        PyObject *tmp;
        tmp = PyFloat_FromDouble( req.fsm_x_nm );
        PyDict_SetItemString( kwargs, "fsm_x_nm", tmp );
        Py_DECREF( tmp );
        tmp = PyFloat_FromDouble( req.fsm_y_nm );
        PyDict_SetItemString( kwargs, "fsm_y_nm", tmp );
        Py_DECREF( tmp );
        tmp = PyFloat_FromDouble( req.exp );
        PyDict_SetItemString( kwargs, "exp", tmp );
        Py_DECREF( tmp );
        tmp = PyFloat_FromDouble( req.gain );
        PyDict_SetItemString( kwargs, "gain", tmp );
        Py_DECREF( tmp );
        tmp = PyFloat_FromDouble( req.blacklevel );
        PyDict_SetItemString( kwargs, "blacklevel", tmp );
        Py_DECREF( tmp );
        tmp = PyLong_FromLong( req.bitdepth );
        PyDict_SetItemString( kwargs, "bitdepth", tmp );
        Py_DECREF( tmp );
        tmp = PyBool_FromLong( req.shutter_closed ? 1 : 0 );
        PyDict_SetItemString( kwargs, "shutter_closed", tmp );
        Py_DECREF( tmp );
        tmp = PyBool_FromLong( req.use_vortex ? 1 : 0 );
        PyDict_SetItemString( kwargs, "use_vortex", tmp );
        Py_DECREF( tmp );
        tmp = PyFloat_FromDouble( req.vmag );
        PyDict_SetItemString( kwargs, "vmag", tmp );
        Py_DECREF( tmp );
        tmp = PyLong_FromLong( req.roi_w );
        PyDict_SetItemString( kwargs, "roi_w", tmp );
        Py_DECREF( tmp );
        tmp = PyLong_FromLong( req.roi_h );
        PyDict_SetItemString( kwargs, "roi_h", tmp );
        Py_DECREF( tmp );
        tmp = PyUnicode_FromString( req.camlo ? "camlo" : "camsci" );
        PyDict_SetItemString( kwargs, "which", tmp );
        Py_DECREF( tmp );

        PyObject *res = PyObject_Call( fn, args, kwargs );
        Py_DECREF( fn );
        Py_DECREF( args );
        Py_DECREF( kwargs );
        Py_DECREF( dmObj );
        Py_DECREF( opdObj );

        if( !res )
        {
            lastError = "optical_bridge.snap failed: " + pyErrString();
            return false;
        }
        if( !PyArray_Check( res ) )
        {
            lastError = "snap did not return a numpy array";
            Py_DECREF( res );
            return false;
        }
        PyArrayObject *arr = reinterpret_cast<PyArrayObject *>( res );
        PyObject *owned = nullptr;
        if( PyArray_TYPE( arr ) != NPY_FLOAT32 || !PyArray_IS_C_CONTIGUOUS( arr ) )
        {
            owned = PyArray_FROM_OTF( res, NPY_FLOAT32, NPY_ARRAY_CARRAY );
            Py_DECREF( res );
            if( !owned )
            {
                lastError = "snap array convert failed: " + pyErrString();
                return false;
            }
            arr = reinterpret_cast<PyArrayObject *>( owned );
        }
        const int nd = PyArray_NDIM( arr );
        if( nd < 2 )
        {
            lastError = "snap array ndim < 2";
            Py_DECREF( owned ? owned : res );
            return false;
        }
        h = static_cast<uint32_t>( PyArray_DIM( arr, nd - 2 ) );
        w = static_cast<uint32_t>( PyArray_DIM( arr, nd - 1 ) );
        const size_t n = static_cast<size_t>( w ) * static_cast<size_t>( h );
        if( out.capacity() < n )
            out.reserve( n );
        out.resize( n );
        std::memcpy( out.data(), PyArray_DATA( arr ), n * sizeof( float ) );
        Py_DECREF( owned ? owned : res );
        return true;
    }

    void shutdown()
    {
        if( m_mod )
        {
            Py_DECREF( m_mod );
            m_mod = nullptr;
        }
        // Do not Py_Finalize(): cupy/CUDA often hang on interpreter teardown.
        m_ready = false;
    }

  private:
    bool m_ready{ false };
    PyObject *m_mod{ nullptr };
};

} // namespace llowfscsim

#endif
