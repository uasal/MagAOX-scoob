/** \file NvInfer.h
 * \brief Test stub for the TensorRT `NvInfer.h` header, declaring only what dlDataCollection uses.
 * \author Claude Code
 *
 * dlDataCollection includes `NvInfer.h` and has `using namespace nvinfer1;`, but calls no TensorRT API,
 * so this stub only declares the namespace.  It shadows any installed TensorRT, which the test build
 * does not put on the include path.
 */

#ifndef dlDataCollection_tests_stubs_NvInfer_h
#define dlDataCollection_tests_stubs_NvInfer_h

/// The TensorRT namespace (empty in this stub).
namespace nvinfer1
{
} // namespace nvinfer1

#endif // dlDataCollection_tests_stubs_NvInfer_h
