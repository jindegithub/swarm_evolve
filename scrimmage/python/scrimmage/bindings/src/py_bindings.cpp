/*
 * ---------------------------------------------------------------------------
 * @section LICENSE
 *  
 * Copyright (c) 2016 Georgia Tech Research Institute (GTRI) 
 *               All Rights Reserved
 *  
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 * ---------------------------------------------------------------------------
 * @file filename.ext
 * @author Kevin DeMarco <kevin.demarco@gtri.gatech.edu> 
 * @author Eric Squires <eric.squires@gtri.gatech.edu>
 * @version 1.0
 * ---------------------------------------------------------------------------
 * @brief A brief description.
 * 
 * @section DESCRIPTION
 * A long description.
 * ---------------------------------------------------------------------------
 */
#include <pybind11/pybind11.h>
#include <py_utils.h>
#include <memory>

PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>)

// see: http://pybind11.readthedocs.io/en/master/faq.html#how-can-i-reduce-the-build-time
PYBIND11_MODULE(py_bindings, m) {
    m.doc() = "pybind11 example plugin";

    add_math(m);
    add_network(m);
    add_common(m);
    add_autonomy(m);

    m.def("frames2pandas", &frames2pandas, "converts a protobuf frames.bin file to a pandas DataFrame");
}
