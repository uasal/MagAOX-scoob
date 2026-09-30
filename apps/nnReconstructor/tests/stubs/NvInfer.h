/** \file NvInfer.h
 * \brief Test stub for the TensorRT `NvInfer.h` header, declaring only what nnReconstructor uses.
 * \author Claude Code
 *
 * Unlike the real TensorRT 10 API (non-virtual wrappers around an implementation pointer), the interfaces here
 * are abstract classes, so that `nnReconstructor_test.cpp` can supply fake runtime, engine and execution-context
 * implementations.  `createInferRuntime()` is defined in the test source.  Only the members nnReconstructor calls
 * are declared, with the signatures it uses.
 */

#ifndef nnReconstructor_tests_stubs_NvInfer_h
#define nnReconstructor_tests_stubs_NvInfer_h

#include <cstddef>
#include <cstdint>

/// The TensorRT namespace.
namespace nvinfer1
{

/// Application-implemented logging interface for TensorRT.
class ILogger
{
  public:
    /// The severity of a TensorRT message.
    enum class Severity : int32_t
    {
        kINTERNAL_ERROR = 0, ///< An internal error has occurred.
        kERROR          = 1, ///< An application error has occurred.
        kWARNING        = 2, ///< An application error has been discovered, but TensorRT has recovered.
        kINFO           = 3, ///< Informational messages.
        kVERBOSE        = 4  ///< Verbose messages with debugging information.
    };

    /// Called by TensorRT to report a message.
    virtual void log( Severity    severity, /**< [in] the severity of the message */
                      const char *msg /**< [in] the null-terminated message text */ ) noexcept = 0;

    /// Destructor.
    virtual ~ILogger() = default;
};

/// A tensor shape with up to 8 dimensions.
class Dims64
{
  public:
    static constexpr int32_t MAX_DIMS{ 8 }; ///< The maximum number of dimensions.

    int32_t nbDims{ 0 }; ///< The number of dimensions.

    int64_t d[MAX_DIMS]{}; ///< The extent of each dimension.
};

/// The tensor shape type, as in TensorRT 10.
using Dims = Dims64;

/// Context for executing inference with an engine.
class IExecutionContext
{
  public:
    /// Destructor.
    virtual ~IExecutionContext() noexcept = default;

    /// Synchronously execute inference on a batch.
    /**
     * \returns true if execution succeeded
     */
    virtual bool
    executeV2( void *const *bindings /**< [in] device pointers of the input and output buffers */ ) noexcept = 0;
};

/// A deserialized inference engine.
class ICudaEngine
{
  public:
    /// Destructor.
    virtual ~ICudaEngine() noexcept = default;

    /// Create an execution context.
    /**
     * \returns the new context, owned by the caller, or nullptr on error
     */
    virtual IExecutionContext *createExecutionContext() noexcept = 0;

    /// Get the number of input and output tensors.
    /**
     * \returns the number of IO tensors
     */
    virtual int32_t getNbIOTensors() const noexcept = 0;

    /// Get the name of an IO tensor.
    /**
     * \returns the tensor name
     */
    virtual const char *getIOTensorName( int32_t index /**< [in] the IO tensor index */ ) const noexcept = 0;

    /// Get the shape of an IO tensor.
    /**
     * \returns the tensor shape
     */
    virtual Dims getTensorShape( const char *tensorName /**< [in] the tensor name */ ) const noexcept = 0;
};

/// Deserializes engines.
class IRuntime
{
  public:
    /// Destructor.
    virtual ~IRuntime() noexcept = default;

    /// Deserialize an engine from host memory.
    /**
     * \returns the engine, owned by the caller, or nullptr on error
     */
    virtual ICudaEngine *
    deserializeCudaEngine( const void *blob, /**< [in] the serialized engine */
                           std::size_t size /**< [in] the size of the blob, in bytes */ ) noexcept = 0;
};

/// Create a runtime.
/**
 * \returns the runtime, owned by the caller, or nullptr on error
 */
IRuntime *createInferRuntime( ILogger &logger /**< [in] the logger to use */ ) noexcept;

} // namespace nvinfer1

#endif // nnReconstructor_tests_stubs_NvInfer_h
