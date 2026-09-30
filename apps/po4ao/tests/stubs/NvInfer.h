/** \file NvInfer.h
 * \brief Test stub for the TensorRT `NvInfer.h` header, declaring only what po4ao uses.
 * \author Claude Code
 *
 * The interfaces are abstract classes with virtual destructors, so that the test can supply fake runtimes, engines
 * and execution contexts, and so that po4ao's `delete` of engine objects runs the fakes' destructors.
 * `createInferRuntime()` is defined in `po4ao_test.cpp`.
 */

#ifndef po4ao_tests_stubs_NvInfer_h
#define po4ao_tests_stubs_NvInfer_h

#include <cstddef>
#include <cstdint>

/// Stand-in for the TensorRT namespace.
namespace nvinfer1
{

/// Stand-in for the TensorRT 10 tensor-shape class.
class Dims64
{
  public:
    /// The maximum number of dimensions.
    static constexpr int32_t MAX_DIMS{ 8 };

    /// The number of dimensions in use.
    int32_t nbDims{ 0 };

    /// The extent of each dimension.
    int64_t d[MAX_DIMS]{};
};

/// The tensor-shape type, an alias for Dims64 as in TensorRT 10.
using Dims = Dims64;

/// Stand-in for the TensorRT logger interface.
class ILogger
{
  public:
    /// The message severity levels, in increasing verbosity.
    enum class Severity : int32_t
    {
        kINTERNAL_ERROR = 0, ///< An internal error.
        kERROR          = 1, ///< An application error.
        kWARNING        = 2, ///< A warning.
        kINFO           = 3, ///< Informational message.
        kVERBOSE        = 4  ///< Verbose message.
    };

    /// Log a message.
    virtual void log( Severity    severity /**< [in] the message severity */,
                      const char *msg /**< [in] the message text */ ) noexcept = 0;

    /// Default c'tor.
    ILogger() = default;

    /// Virtual d'tor.
    virtual ~ILogger() = default;
};

/// Stand-in for the TensorRT execution context interface.
class IExecutionContext
{
  public:
    /// Virtual d'tor.
    virtual ~IExecutionContext() = default;

    /// Run synchronous inference.
    /**
     * \returns true on success
     */
    virtual bool executeV2( void *const *bindings /**< [in] device buffers, inputs then outputs */ ) noexcept = 0;
};

/// Stand-in for the TensorRT engine interface.
class ICudaEngine
{
  public:
    /// Virtual d'tor.
    virtual ~ICudaEngine() = default;

    /// Create an execution context for this engine.
    /**
     * \returns the new context, owned by the caller
     */
    virtual IExecutionContext *createExecutionContext() noexcept = 0;

    /// Get the number of input and output tensors.
    virtual int32_t getNbIOTensors() const noexcept = 0;

    /// Get the name of an input or output tensor.
    virtual const char *getIOTensorName( int32_t index /**< [in] the tensor index */ ) const noexcept = 0;

    /// Get the shape of a tensor.
    virtual Dims getTensorShape( const char *tensorName /**< [in] the tensor name */ ) const noexcept = 0;
};

/// Stand-in for the TensorRT runtime interface.
class IRuntime
{
  public:
    /// Virtual d'tor.
    virtual ~IRuntime() = default;

    /// Deserialize an engine from a serialized blob.
    /**
     * \returns the engine, owned by the caller, or nullptr on error
     */
    virtual ICudaEngine *deserializeCudaEngine( const void *blob /**< [in] the serialized engine */,
                                                std::size_t size /**< [in] the blob size in bytes */ ) noexcept = 0;
};

/// Create a TensorRT runtime.
/**
 * \returns the runtime, owned by the caller, or nullptr on error
 */
IRuntime *createInferRuntime( ILogger &logger /**< [in] the logger used by the runtime */ ) noexcept;

} // namespace nvinfer1

#endif // po4ao_tests_stubs_NvInfer_h
