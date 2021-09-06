#pragma once
#include <sys/ioctl.h>
#include <type_traits>
#include <string>
#include <algorithm>
#include "exception.hpp"
#include "util.hpp"
#include "meta.hpp"
#include "varargs.hpp"

namespace jab{
namespace file{

enum flags{
    r = 1,
    w = r << 1,
    rw = r | w,
    noctty = w << 1,
    ndelay = noctty << 1,
    nonblock = ndelay << 1
};

struct File_state{
    using fd_t = int;
    fd_t m_fd = -1;
    bool m_debug = false;
};

class File{
public:
    using fd_t = File_state::fd_t;
    using len_t = ::size_t;

private:
    File_state m_state;
    
protected:
    //Use the correct one depending on whether it can alter visible state
    template<typename... Args>
    int const_ioctl(unsigned long request, Args... args) const{
        //Despite the wide variation in ioctl semantics, -1 is described as the error return
        //and it is specified that meaningful non-error returns are >= 0
        int result = ::ioctl(*this, request, std::forward<Args>(args)... );
        if(result == -1) throw jab::exception::posix_exception("ioctl() failed");
        return result;
    }
    
    template<typename... Args>
    int ioctl(unsigned long request, Args... args){
        return const_ioctl(request, std::forward<Args>(args)...);
    }
    
    //What we do upon destruction
    //When implementing from a derived class remember that all derived classes
    //have been torn down when this get called, so "this" should be treated as File *
    virtual void cleanup();
    
public:
    static constexpr fd_t null_fd = -1;
    File();
    File(fd_t fd);    
    ~File();
    File(File &&);
    
    //Could be implemented with dup(), but I don't feel like it yet
    File &operator=(const File &) = delete;
    
    //Fundamental I/O operations
    virtual void close();
    virtual ::ssize_t read(char *data, len_t len);
    virtual ::ssize_t write(const char *data, len_t len);
    virtual size_t available() const;
    virtual void flush();
    
    operator bool() const;
    operator fd_t() const;
    File &operator=(File &&);
    len_t read_exactly(char *data, len_t len);
    void write_exactly(const char *data, len_t len);
    std::string read_until(char c);
    void debug(bool v);
    bool debug() const;
    
    template<typename T>
    void write( const jab::util::as_bin<T> &obj ){
        using type = typename jab::util::as_bin<T>::type;
        using traits = jab::util::binary_traits<type>;
        write_exactly( traits::data(obj), traits::length(obj) );
    }
};

//stdin, stdout, stderr
class StdFile:public File{
public:
    static StdFile stdin, stdout, stderr;

    using File::File;
    
    //flush is NOP on stdio streams?
    void flush() override;
    
    //Let the runtime or someone else close these
    void cleanup() override;
};


class FDSet{
  ::fd_set m_fds;
  mutable File::fd_t m_fd_max;
  mutable bool m_max_valid;
  
  //Find out if any argument is equal to m_fd_max
  template<typename... Args>
  bool has_max(Args... arg){
      return logical_or_varargs( arg == m_fd_max... );
  }
  
  void set_impl(File::fd_t );
  void clear_impl(File::fd_t);
  int isset_impl(File::fd_t) const;
  void find_max() const;

  template<typename T>
  using converts_to_fd = typename std::enable_if< std::is_convertible<T, File::fd_t>::value, T >::type;
  
protected:
    friend int select(FDSet &, FDSet &, FDSet&, ::timeval *);
    
    //max fds is no longer known while a non-const pointer is out for use
    ::fd_set *invalidate_max();
 
public:
    FDSet();
    
    template<typename... Args, typename = std::void_t<converts_to_fd<Args>...> >
    FDSet(Args &...  args):FDSet(){
        set(args...);
    }
    
    template<typename... Args, typename = std::void_t<converts_to_fd<Args>...>>
    void set(Args &... args){
        jab::meta::null_function((set_impl(args), 0)... );
    }
    
    template<typename... Args, typename = std::void_t<converts_to_fd<Args>...>>
    void clear(Args &... args){
        
        jab::meta::null_function((clear_impl(args), 0)...);
        
        //If we cleared the max fd, then find the new max fd. Something like 1k comparisons.
        if(has_max(args...))
            m_max_valid = false; //lazy
    }
    
    //Count of fds present
    template<typename... Args, typename = std::void_t<converts_to_fd<Args>...>>
    int isset(Args &... args){
        return jab::va::sum( isset_impl(args) ... );
    }
    
    void zero();
    File::fd_t max_fd() const;
    
    //fd_set *fds();
    const fd_set *fds() const;
};


//Maybe unnecessary? What if timeval isn't always just two fields?
struct timeval:public ::timeval{
    using sec_t = decltype(::timeval::tv_sec);
    using usec_t = decltype(::timeval::tv_usec);
    
    timeval(sec_t sec, usec_t usec);
};

int select(FDSet &rfds, FDSet &wfds, FDSet &efds, ::timeval *timeout);

template<class Rfds, class Wfds, class Efds, class TV, typename = 
    std::void_t<
        meta::const_optional< typename std::remove_reference<Rfds>::type, FDSet>,
        meta::const_optional< typename std::remove_reference<Wfds>::type, FDSet>,
        meta::const_optional< typename std::remove_reference<Efds>::type, FDSet>,
        meta::permit_if_converts_to<TV, ::timeval>
>>
int select(Rfds &&rfds, Wfds &&wfds, Efds &&efds, TV &&timeout){
    
    util::deconst rfdsd(std::forward<Rfds>(rfds)), wfdsd(std::forward<Wfds>(wfds)), efdsd(std::forward<Wfds>(efds));
    util::deconst tvd(std::forward<TV>(timeout));
    
    return file::select(rfdsd.get(), wfdsd.get(), efdsd.get(), &tvd.get());
}

template<class Rfds, class Wfds, class Efds, typename = 
    std::void_t<
        meta::const_optional< typename std::remove_reference<Rfds>::type, FDSet>,
        meta::const_optional< typename std::remove_reference<Wfds>::type, FDSet>,
        meta::const_optional< typename std::remove_reference<Efds>::type, FDSet>
>>
int select(Rfds &&rfds, Wfds &&wfds, Efds &&efds){
    util::deconst rfdsd(std::forward<Rfds>(rfds)), wfdsd(std::forward<Wfds>(wfds)), efdsd(std::forward<Wfds>(efds));
    return file::select(rfdsd.get(), wfdsd.get(), efdsd.get(), NULL);
}

template< typename Char, class Traits = std::char_traits<Char> >
class Filestreambuf : public std::basic_streambuf<Char, Traits>{
    using base_type = std::basic_streambuf<Char, Traits>;
    using char_type = Char;
    using streamsize = base_type::streamsize;
    using pos_type = base_type::pos_type;
    using off_type = base_type::off_type;
    using traits_type = base_type::traits_type;
    using int_type = base_type::int_type;

    char_type m_rbuf_bak, m_wbuf_bak; //single-character fallback buffers in case none are provided
    char_type *m_rbuf, *m_wbuf;
    streamsize m_buflen;
    File &m_file;

    void reset_readbuf(){
        setg(m_rbuf, m_rbuf, m_rbuf);
    }

public:
    Filestreambuf(File &file):
        m_file(file){

        setbuf(nullptr, 0);
    }

    streamsize getbufavail() const{
        return this->egptr() - this->gptr();
    }

    //TODO: Attempt to presrve the previous buffer data, because right now, this will purge it
    virtual Filestreambuf* setbuf (char* s, streamsize n) override{
        auto half = n / 2;
        if( half < 1 ){            
            m_rbuf = &m_rbuf_bak;
            m_wbuf = &m_wbuf_bak;
            m_buflen = 1;
        }
        else{
            m_rbuf = s;
            m_wbuf = &s[half];
            m_buflen = half;            
        }   

        reset_readbuf();
        setp(m_wbuf, m_wbuf);
    }

    virtual pos_type seekpos (pos_type pos, std::ios_base::openmode which) override{
        if(which == std::ios_base::in){
            setg(this->eback(), this->eback() + pos, this->egptr());
            return pos;
        }

        if(which == std::ios_base::out){
            //This is kind of stupid, but setp resets the current output pointer to the start of the sequence,
            //so we set setp() to its current position for that side-effect.
            //So, from there, you can bump it to an absolute position. Otherwise, I don't know how else to
            //rewind it.
            setp( this->pbase(), this->epptr() );
            pbump( pos );
        }
        return pos;
    }

    virtual pos_type seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which) override{
        
        if(which == std::ios_base::in){
            char_type *newpos;

            switch(way){
                case std::ios_base::beg:
                    newpos = this->eback() + off;
                    break;

                case std::ios_base::end:
                    newpos = this->egptr() - off;
                    break;

                case std::ios_base::cur:
                    newpos = this->gptr() + off;
                    break;
            }
            setg(this->eback(), newpos, this->egptr());
            return newpos - m_rbuf;
        }

        if(which == std::ios_base::out){

            //Convert to an absolute position and just use seekpos
            pos_type newpos, curpos = this->pptr() - this->pbase();

            switch(way){
                case std::ios_base::beg:
                    newpos = off;
                    break;

                case std::ios_base::end:
                    newpos = this->egptr() - off;
                    break;

                case std::ios_base::cur:
                    this->pbump(off);
                    return this->pptr() - this->pbase();
            }

            return seekpos(newpos, std::ios_base::out);
        }
    }

    virtual int sync() override{
        try{
            m_file.flush();
            return 0;
        }
        catch(...){
            return -1;
        }
    }

    virtual streamsize showmanyc() override{
        return m_file.available();
    }

    virtual streamsize xsgetn(char_type* s, streamsize n) override{
        streamsize ct = 0, bufn = std::min(n, getbufavail());
        std::copy( this->gptr(), &this->gptr()[bufn], s );
        this->gbump(bufn);        

        //buffer underflow case
        if( this->gptr() >= this->egptr() ){
            n -= bufn;
            s += bufn;

            //If the buffer will just overflow again, then bulk-read straight into the destination
            if( n > m_buflen ){                
                ct = m_file.read_exactly(s, n);                
            }
            else{
                if(this->underflow() != traits_type::eof())
                    ct = this->xsgetn(s, n);
            }
        }

        return bufn + ct;
    }    

    //The buffer has to be larger than the maximum read size to ensure that there is always one putback space
    virtual int_type pbackfail (int_type c) override{
        if( this->egptr() - this->eback() >= m_buflen )
            return traits_type::eof;
        
        std::copy_backward( this->eback(), this->egptr(), this->egptr() + 1);
        *this->gptr() = c;
        return c;
    }

    virtual streamsize xsputn(const char_type* s, streamsize n) override{
        auto space = (m_wbuf + m_buflen) - this->epptr();
        auto n  = std::min(n, space);

        std::copy( s, s + n, this->epptr());
        this->setp( this->pbase(), this->epptr() + n );
        return n;
    }

    virtual int_type overflow (int_type c) override{
        
    }

};

}
}
