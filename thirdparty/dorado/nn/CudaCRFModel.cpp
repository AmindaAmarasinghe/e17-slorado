#include "CudaCRFModel.h"

#include "dorado/decode/GPUDecoder.h"
#include "error.h"

#include "../../../src/globals.h"
#include "../../../src/misc.h"

#include <c10/cuda/CUDAGuard.h>
#include <c10/cuda/CUDAStream.h>
#include "toml.h"
#include <torch/torch.h>

using namespace std::chrono_literals;

class CudaCaller {
public:
    CudaCaller(const std::string &model_path,
               int chunk_size,
               int batch_size,
               const std::string &device) {
                // std::cout << "\nCuda_CRF 22\n" << std::endl; //Test
        // isCUDA = true;
        CudaCallerT -= realtime();
        // CudaCallerT1 -= realtime();
        const auto model_config = load_crf_model_config(model_path);
        
        m_model_stride = static_cast<size_t>(model_config.stride);
        // CudaCallerT1 += realtime();
        // CudaCallerT2 -= realtime();
        m_decoder_options = DecoderOptions();
        // CudaCallerT2 += realtime();
        // CudaCallerT3 -= realtime();
        m_decoder_options.q_shift = model_config.qbias;
        m_decoder_options.q_scale = model_config.qscale;
        // CudaCallerT3 += realtime();
        // CudaCallerT4 -= realtime();
        m_decoder = std::make_unique<GPUDecoder>();
        m_num_input_features = model_config.num_features;
        // CudaCallerT4 += realtime();

        m_options = torch::TensorOptions().dtype(GPUDecoder::dtype).device(device);
       
        CudaCallerT5 -= realtime();
        m_module = load_crf_model(model_path, model_config, batch_size, chunk_size, m_options);
        CudaCallerT5 += realtime();
        // CudaCallerT6 -= realtime();
        m_cuda_thread.reset(new std::thread(&CudaCaller::cuda_thread_fn, this));
        // CudaCallerT6 += realtime();
        CudaCallerT += realtime();

        // CudaCallerT += getSubTimeDifference();
    }

    ~CudaCaller() {
        NCudaCallerT -= realtime();
        std::unique_lock<std::mutex> input_lock(m_input_lock);
        m_terminate = true;
        input_lock.unlock();
        m_input_cv.notify_one();
        m_cuda_thread->join();
        NCudaCallerT += realtime();
        // NCudaCallerT += getTimeDifference();
    }

    struct NNTask {
        // std::cout << "\nCuda_CRF 56\n" << std::endl; //Test
        // startTime = realtime();
        NNTask(torch::Tensor input_, int num_chunks_) : input(input_), num_chunks(num_chunks_) {}
        torch::Tensor input;
        std::mutex mut;
        std::condition_variable cv;
        torch::Tensor out;
        bool done{false};
        
        int num_chunks;
        // endTime = realtime();
        // NNTaskT += getTimeDifference();
    };

    std::vector<DecodedChunk> call_chunks(torch::Tensor &input,
                                          torch::Tensor &output,
                                          int num_chunks,
                                          c10::cuda::CUDAStream stream) {
                                            // std::cout << "\nCuda_CRF 73\n" << std::endl; //Test
        call_chunksT -= realtime();
        c10::cuda::CUDAStreamGuard stream_guard(stream);

        if (num_chunks == 0) {
            return std::vector<DecodedChunk>();
        }

        // NNTaskT0 -= realtime();
        NNTask task(input.to(m_options.device()), num_chunks);
        // NNTaskT0 += realtime();
        // NNTaskT1 -= realtime();
        {
            std::lock_guard<std::mutex> lock(m_input_lock);
            m_input_queue.push_front(&task);
        }
        m_input_cv.notify_one();
        // NNTaskT1 += realtime();
        
        std::unique_lock<std::mutex> lock(task.mut);

        while (!task.done) {
            NNTaskT2 -= realtime();
            task.cv.wait(lock);
            NNTaskT2 += realtime();

        }
        // NNTaskT2 += realtime();


        output.copy_(task.out);
        // endTime = realtime();
        call_chunksT += realtime();
        // call_chunksT += getTimeDifference();
        return m_decoder->cpu_part(output);
    }

    void cuda_thread_fn() {
        // std::cout << "\nCuda_CRF 119\n" << std::endl; //Test
        // startTime = realtime();
        torch::InferenceMode guard;
        c10::cuda::CUDAGuard device_guard(m_options.device());
        auto stream = c10::cuda::getCurrentCUDAStream(m_options.device().index());

        while (true) {
            // cuda_thread_fnT -= realtime();
            std::unique_lock<std::mutex> input_lock(m_input_lock);
            while (m_input_queue.empty() && !m_terminate) {
                if(m_input_queue.size() > 0){            
                    std::cout << "\nlength: " << m_input_queue.size() << "\n" << std::endl; //Test
                }
                m_input_cv.wait_for(input_lock, 100ms);
            }
            // TODO: finish work before terminating?
            // cuda_thread_fnT += realtime();
            if (m_terminate) {
                return;
            }
            cuda_thread_fnT2 -= realtime();

            // cuda_thread_fnT3 -= realtime();
            NNTask *task = m_input_queue.back();
            // cuda_thread_fnT3 += realtime();
            // cuda_thread_fnT4 -= realtime();
            m_input_queue.pop_back();
            input_lock.unlock();
            // cuda_thread_fnT4 += realtime();


            std::unique_lock<std::mutex> task_lock(task->mut);
            cuda_thread_fnT5 -= realtime();
            auto scores = m_module->forward(task->input);
            cuda_thread_fnT5 += realtime();
            cuda_thread_fnT6 -= realtime();
            torch::cuda::synchronize();
            task->out = m_decoder->gpu_part(scores, task->num_chunks, m_decoder_options, m_device);
            cuda_thread_fnT6 += realtime();
            stream.synchronize();
            task->done = true;
            task->cv.notify_one();
            task_lock.unlock();
            cuda_thread_fnT2 += realtime();

        }
        // endTime = realtime();
        // cuda_thread_fnT += getTimeDifference();
    }

    // startTime = realtime();
    // std::cout << "\nCuda_CRF 153\n" << std::endl; //Test
    std::string m_device;
    torch::TensorOptions m_options;
    std::unique_ptr<GPUDecoder> m_decoder;
    DecoderOptions m_decoder_options;
    torch::nn::ModuleHolder<torch::nn::AnyModule> m_module{nullptr};
    size_t m_model_stride;
    bool m_terminate{false};
    std::deque<NNTask *> m_input_queue;
    std::mutex m_input_lock;
    std::condition_variable m_input_cv;
    std::unique_ptr<std::thread> m_cuda_thread;
    int m_num_input_features;
    // endTime = realtime();
    // SubCudaCallerT += getTimeDifference();
};

std::shared_ptr<CudaCaller> create_cuda_caller(const std::string &model_path,
                                               int chunk_size,
                                               int batch_size,
                                               const std::string &device) {
                                                // std::cout << "\nCuda_CRF 174\n" << std::endl; //Test
    return std::make_shared<CudaCaller>(model_path, chunk_size, batch_size, device);
}

CudaModelRunner::CudaModelRunner(std::shared_ptr<CudaCaller> caller, int chunk_size, int batch_size)
        : m_caller(caller),
          m_stream(c10::cuda::getStreamFromPool(false, m_caller->m_options.device().index())) {
            // std::cout << "\nCuda_CRF 181\n" << std::endl; //Test
    // adjust chunk size to be a multiple of the stride
    chunk_size -= chunk_size % model_stride();

    m_input = torch::empty({batch_size, caller->m_num_input_features, chunk_size},
                           torch::TensorOptions()
                                   .dtype(m_caller->m_options.dtype())
                                   .device(torch::kCPU)
                                   .pinned_memory(true));

    long int block_size = chunk_size / model_stride();
    m_output = torch::empty(
            {3, batch_size, block_size},
            torch::TensorOptions().dtype(torch::kInt8).device(torch::kCPU).pinned_memory(true));
    // warm up
    call_chunks(batch_size);
}

void CudaModelRunner::accept_chunk(int chunk_idx, at::Tensor slice) {
    // std::cout << "\nCuda_CRF 200\n" << std::endl; //Test
    m_input.index_put_({chunk_idx, torch::indexing::Ellipsis}, slice);
}

std::vector<DecodedChunk> CudaModelRunner::call_chunks(int num_chunks) {
    // std::cout << "\nCuda_CRF 205\n" << std::endl; //Test
    return m_caller->call_chunks(m_input, m_output, num_chunks, m_stream);
}

size_t CudaModelRunner::model_stride() const { return m_caller->m_model_stride; }
size_t CudaModelRunner::chunk_size() const { return m_input.size(2); }
// std::cout << "\nCuda_CRF 211\n" << std::endl; //Test
