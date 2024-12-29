#pragma once
#include "common.h"
#include <vector>
#include <cstring>
namespace Argus {


template<typename T>
inline typename std::enable_if<!std::is_same<T, PARAM_TYPE>::value &&( std::is_same<T, const char*>::value || std::is_same<T, char*>::value), T>::type
decode_param_reverse(int &start_ridx , int total_size, const log_info& info,  std::vector<char *> &vect_buff)
{
    start_ridx ++ ;
    int left_idx =(total_size - start_ridx);  //tricks for args parsed from right most, so we could do this

    return reinterpret_cast<T>(vect_buff[left_idx]);
}

template <typename T>
inline typename std::enable_if<!std::is_same<T, PARAM_TYPE>::value
                                   && !std::is_same<T, const char *>::value
                                   && !std::is_same<T, char *>::value,
                               T>::type
decode_param_reverse(int &start_ridx, int total_size, const log_info &info, char* data)
{
    auto &val = *reinterpret_cast<T*>(data - start_ridx);
    start_ridx -= sizeof(T);
    return val;
}

template<typename T>
inline typename std::enable_if<!std::is_same<T, PARAM_TYPE>::value &&( std::is_same<T, const char*>::value || std::is_same<T, char*>::value), T>::type
decode_param(int &start_ridx , int total_size, const log_info& info,  std::vector<char *> &vect_buff)
{
    start_ridx ++ ;
    int left_idx =(total_size - start_ridx);

    return reinterpret_cast<T>(vect_buff[left_idx]);
}

template <typename T>
inline typename std::enable_if<!std::is_same<T, PARAM_TYPE>::value
                                   && !std::is_same<T, const char *>::value
                                   && !std::is_same<T, char *>::value,
                               T>::type
decode_param(int &start_ridx, int total_size, const log_info &info, std::vector<char *> &vect_buff)
{
    start_ridx ++ ;
    int left_idx =(total_size - start_ridx);

    if (left_idx >= (int)vect_buff.size()) {
        return 0;
    } else {
        T argument = *reinterpret_cast<T *>(vect_buff[left_idx]);
        return argument;
    }
}

/**********************************************************************************************
* we have to do this copy because the unpack seqence into fprintf like va_last setps from right
* if we doing this asynchronously we may skip this
*/
template<typename T>
bool copy_single_arg(const log_info&info,char**input, std::vector<char *> &vector4cache, char **printfBuf, uint32_t fragment_size, uint32_t&buf_cpy_len)
{
    
    int curIndex = vector4cache.size();
    auto param_type = info.param_types[curIndex];
    char * write_addr = (*printfBuf) + buf_cpy_len;

    if (param_type != PARAM_TYPE::STRING) {
        if ((buf_cpy_len + sizeof(T)) > fragment_size){
            return false;
        }
        *reinterpret_cast<T*>(write_addr) = *reinterpret_cast<T*>(*input);
        buf_cpy_len += sizeof(T);
        *input += sizeof(T);
        vector4cache.push_back(write_addr);

    } else if (param_type == PARAM_TYPE::STRING) {
        uint32_t str_size = *reinterpret_cast<uint32_t*>(*input);

        if ((buf_cpy_len + str_size + 1) > fragment_size){
            return false;
        }
        *input += sizeof(uint32_t);
        memcpy(write_addr, *input,str_size);
        write_addr[str_size] = '\0';
        *input += str_size ;
        buf_cpy_len += str_size + 1;
        vector4cache.push_back(write_addr);
    }
    return true;
}


template<typename... Ts>
bool reverse_from_buffer(const log_info&info,char**input, std::vector<char *> &vector4cache, char **printfBuf, uint32_t fragment_size, uint32_t &buf_cpy_len);

template<>
inline bool reverse_from_buffer(const log_info&info,char**input, std::vector<char *> &vector4cache, char **printfBuf, uint32_t fragment_size, uint32_t &buf_cpy_len)
{
    return true;
}

template<typename Head, typename... Ts>
inline bool copy_helper(const log_info&info,char**input, std::vector<char *> &vector4cache, char **printfBuf, uint32_t fragment_size, uint32_t &buf_cpy_len)
{
    if (!copy_single_arg<Head>(info, input, vector4cache, printfBuf, fragment_size, buf_cpy_len))
    {
        return false;
    }
    return reverse_from_buffer<Ts...>(info, input, vector4cache, printfBuf, fragment_size, buf_cpy_len);
}


template<typename... Ts>
bool reverse_from_buffer(const log_info&info,char**input, std::vector<char *> &vector4cache, char **printfBuf, uint32_t fragment_size, uint32_t &buf_cpy_len)
{
    return copy_helper<Ts...>(info,input, vector4cache, printfBuf, fragment_size, buf_cpy_len);

}


/*
 * fixed length type
 */
template<typename T>
inline typename std::enable_if<!std::is_same<T, const wchar_t*>::value
                        && !std::is_same<T, const char*>::value
                        && !std::is_same<T, wchar_t*>::value
                        && !std::is_same<T, char*>::value
                        && !std::is_same<T, const void*>::value
                        && !std::is_same<T, void*>::value
                        , size_t>::type
calc_one_arg(const PARAM_TYPE fmtType,
           
           size_t &arg_size,
           T arg)
{
    return sizeof(T);
}


/*
 * string type
 */
inline size_t calc_one_arg(const PARAM_TYPE fmtType,
                           size_t &arg_bytes,
                           const char *str)
{
    if (fmtType != PARAM_TYPE::STRING)
        return sizeof(void *);
#ifndef DUMP_REVERSE
    arg_bytes = strlen(str);
#else
    arg_bytes = strlen(str) + 1;
#endif
    return arg_bytes + sizeof(uint32_t);
}

template<int argNum = 0, unsigned long N, int M, typename T1, typename... Ts>
inline size_t
calc_arg_sizes(const std::array<PARAM_TYPE, N>& argFmtTypes,
            size_t (&arg_sizes)[M],
            T1 head, Ts... rest)
{
    return calc_one_arg(argFmtTypes[argNum],
                                                    arg_sizes[argNum], head)
           + calc_arg_sizes<argNum + 1>(argFmtTypes, 
                                                    arg_sizes, rest...);
}

/*
 * 0 arguments
 */
template<int argNum = 0, unsigned long N, int M>
inline size_t
calc_arg_sizes(const std::array<PARAM_TYPE, N>&, size_t (&)[M])
{
    return 0;
}


template<typename T>
inline typename std::enable_if<!std::is_same<T, const wchar_t*>::value
                        && !std::is_same<T, const char*>::value
                        && !std::is_same<T, wchar_t*>::value
                        && !std::is_same<T, char*>::value
                        , void>::type
encode_argument(char **storage,
               T arg,
               PARAM_TYPE paramType,
               size_t arg_size)
{
    std::memcpy(*storage, &arg, sizeof(T));
    *storage += sizeof(T);

}

template<typename T>
inline typename std::enable_if<std::is_same<T, const wchar_t*>::value
                        || std::is_same<T, const char*>::value
                        || std::is_same<T, wchar_t*>::value
                        || std::is_same<T, char*>::value
                        , void>::type
encode_argument(char **storage,
               T arg,
               const PARAM_TYPE paramType,
               const size_t arg_size)
{

    auto size = static_cast<uint32_t>(arg_size);
    std::memcpy(*storage, &size, sizeof(uint32_t));
    *storage += sizeof(uint32_t);

    memcpy(*storage, arg, arg_size);
    *storage += arg_size;
    return;
}


template<int argNum = 0, unsigned long N, int M, typename T1, typename... Ts>
inline void encode_arguments(const std::array<PARAM_TYPE, N>& paramTypes,
                size_t (&arg_bytes)[M],
                char **storage,
                T1 head,
                Ts... rest)
{
    encode_argument(storage, head, paramTypes[argNum], arg_bytes[argNum]);
    encode_arguments<argNum + 1>(paramTypes, arg_bytes, storage, rest...);
}


template<int argNum = 0, unsigned long N, int M>
inline void
encode_arguments(const std::array<PARAM_TYPE, N>&,
                size_t (&arg_sizes)[M],
                char **)
{}

}