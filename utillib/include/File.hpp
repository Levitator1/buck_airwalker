#pragma once
#include <sys/ioctl.h>
#include <type_traits>
#include <memory>
#include <string>
#include <algorithm>
#include "Config.hpp"
#include "exception.hpp"
#include "util.hpp"
#include "meta.hpp"
#include "varargs.hpp"
#include "ringbuf.hpp"

namespace jab{
namespace file{

enum flags{
    r = 1,
    w = r << 1,
    rw = r | w,
    noctty = w << 1,
    ndelay = noctty << 1,
    nonblock = ndelay << 1,
    create = nonblock << 1
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

	//virtual so that the null instance can throw on reassignment
	virtual void move_assign( File &&rhs );

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
    virtual std::streamsize read(char *data, std::streamsize len);
    virtual std::streamsize write(const char *data, std::streamsize len);
	virtual std::streamsize seek(std::streamsize pos, std::ios_base::seekdir dir);
    virtual std::streamsize available() const;
    virtual void flush();
    
    operator bool() const;
    operator fd_t() const;
    File &operator=(File &&);
    std::streamsize read_exactly(char *data, std::streamsize len);
    void write_exactly(const char *data, std::streamsize len);
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

class NullFile:public File{
protected:
	virtual void move_assign(File &&rhs) override;

public:
	using File::File;
};

//Can't really be const because then it's not very useful
//Move-assignment is overloaded for the null version so that you can't accidentally reassign it
//And all other IO operations should throw
extern NullFile null_file;

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

template< typename Char = char, class Traits = std::char_traits<Char> >
class Filestreambuf : public std::basic_streambuf<Char, Traits> {

    using base_type = std::basic_streambuf<Char, Traits>;
    using char_type = Char;    
    using pos_type = base_type::pos_type;
    using off_type = base_type::off_type;
    using traits_type = base_type::traits_type;    
    using int_type = base_type::int_type;
    using streamsize = std::streamsize;

	struct State{
		char_type m_rbuf_bak, m_wbuf_bak; //single-character fallback buffers in case none are provided
		jab::io::ringbuffer<char_type> m_rbuf, m_wbuf;    
		File *m_file;
	} m_state;

    //Synchronize the streambuf get-pointers with the get-ringbuffer
    void update_gptrs(){
        this->setg(m_state.m_rbuf.get_begin(), m_state.m_rbuf.get_begin(), m_state.m_rbuf.get_end());
    }

    //Synchronize the streambuf put-pointers with the put-ringbuffer
    void update_pptrs(){
        this->setp( m_state.m_wbuf.put_begin(), m_state.m_wbuf.put_end());
    }

    //Commit writes in the buffer window to the ringbuffer
    void commit_wbuf(){
        m_state.m_wbuf.push( this->pptr() - this->pbase() );
    }

    //Commit reads in the read-buffer window to the ringbuffer
    void commit_rbuf(){
        m_state.m_rbuf.pop( this->gptr() - this->eback() );
    }
    
    //These guards commit updates to the user-facing end of the ringbuffer
    //And when they go away, they ensure that the corresponding streambuf pointers are synchronized to the ringbuffer
    class GetGuard{
        Filestreambuf &m_buf;
    public:
        GetGuard( Filestreambuf &buf ):
            m_buf(buf){
                m_buf.commit_rbuf();
        }

        ~GetGuard(){
            m_buf.update_gptrs();
        }
    };

    class PutGuard{
        Filestreambuf &m_buf;
    public:
        PutGuard( Filestreambuf &buf ):
            m_buf(buf){
                m_buf.commit_wbuf();
        }

        ~PutGuard(){
            m_buf.update_pptrs();
        }
    };

public:	
    Filestreambuf(File &file = null_file, char_type *buf = nullptr,  std::streamsize bufn = 0):
		m_state({ 0, 0, {}, {}, &file }){

        setbuf(buf, bufn);
    }

	Filestreambuf &operator=(Filestreambuf &&rhs){
		m_state = std::move(rhs.m_state);
		rhs.m_state.m_file = &null_file;
		return *this;
	}

    //streamsize getbufavail() const{
        //return this->egptr() - this->gptr();
    //}

    //TODO: Attempt to presrve the previous buffer data, because right now, this will purge it
    virtual base_type* setbuf (char* s, std::streamsize n) override{
        auto half = n / 2;
        if( half < 1 ){            
            m_state.m_rbuf.buf(&m_state.m_rbuf_bak, 1);
            m_state.m_wbuf.buf(&m_state.m_wbuf_bak, 1);            
        }
        else{
            m_state.m_rbuf.buf(s, half);
            m_state.m_wbuf.buf(&s[half], half);               
        }   

        update_gptrs();
        update_pptrs();
        return this;
    }

    virtual pos_type seekpos (pos_type pos, std::ios_base::openmode which) override{
        if(which == std::ios_base::in){
            this->setg(this->eback(), this->eback() + pos, this->egptr());
            return pos;
        }

        if(which == std::ios_base::out){
            //This is kind of stupid, but setp resets the current output pointer to the start of the sequence,
            //so we set setp() to its current position for that side-effect.
            //So, from there, you can bump pptr() to an absolute position. Otherwise, I don't know how else to
            //rewind it.
            this->setp( this->pbase(), this->epptr() );
            this->pbump( pos );
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
            this->setg(this->eback(), newpos, this->egptr());
            return newpos - this->eback();
        }

        if(which == std::ios_base::out){
            //Convert to an absolute position and just use seekpos
            pos_type newpos, curpos = this->pptr() - this->pbase();

            switch(way){
                case std::ios_base::beg:
                    newpos = off;
                    break;

                case std::ios_base::end:
                    newpos = this->epptr() - this->pbase() - off;
                    break;

                case std::ios_base::cur:
                    this->pbump(off);
                    return this->pptr() - this->pbase();
            }

            return seekpos(newpos, std::ios_base::out);
        }
        return this->egptr() - this->eback();
    }
private:
    streamsize drain_getbuf( char_type *s, streamsize n ){
        streamsize ct = std::min((::size_t)n, m_state.m_rbuf.getavail() );
        std::copy( m_state.m_rbuf.get_begin(), m_state.m_rbuf.get_begin() + ct, s );
        m_state.m_rbuf.pop(ct);
        return ct;
    }

public:
    //These next two only pertain to the underlying file and do not affect the buffering state according to the crappy online references.
	//However, that seems to leave stuff in the buffer when the file closes. So we will flush the streambuf buffer as well.
    virtual int sync() override{        
        try{
			//Twice becasue the ringbuffer has up to two segments
			overflow( traits_type::eof() );
			overflow( traits_type::eof() );
            m_state.m_file->flush();
            return 0;
        }
        catch(...){
            return -1;
        }
    }

    virtual streamsize showmanyc() override{
        return m_state.m_file->available();
    }

    virtual streamsize xsgetn(char_type* s, streamsize n) override{

        streamsize ct, bufn;
        {
            GetGuard gg(*this);

            //Drain the ringbuffer twice because it can have up to 1 discontinuity
            bufn = drain_getbuf(s, n);
            s += bufn;
            n -= bufn;
            ct = drain_getbuf(s, n);
            n -= ct;
            s += ct;
            bufn += ct;
            ct = 0;
        }

        //buffer underflow case
        if( n > 0 ){            
            //If the buffer will just fill up again, then bulk-read straight into the destination
            if( n >= m_state.m_rbuf.capacity() ){                
                ct = m_state.m_file->read(s, n);                
            }
            else{
                if(this->underflow() != traits_type::eof()){
                    ct = drain_getbuf(s, n);
                    update_gptrs();
                }
            }
        }

        return bufn + ct;
    }    

    //The buffer has to be larger than the maximum read size to ensure that there is always one putback space
    virtual int_type pbackfail (int_type c) override{

        GetGuard gg(*this);

        if(m_state.m_rbuf.size() >= m_state.m_rbuf.capacity())
            return traits_type::eof();

        m_state.m_rbuf.unpop();
        *m_state.m_rbuf.get_begin() = c;        
        return c;
    }

private:
    streamsize fill_wbuf(const char_type* s, streamsize n){        
        n = std::min( m_state.m_wbuf.putavail(), (::size_t)n);
        std::copy( s, s+n, m_state.m_wbuf.put_begin() );
        m_state.m_wbuf.push(n);
        return n;
    }

public:
    virtual streamsize xsputn(const char_type* s, streamsize n) override{

        streamsize ct=0, ct2=0;

        PutGuard pg(*this);

        //If the buffer is already partially populated than try to fill it
        if( m_state.m_wbuf.size() ){
            ct = fill_wbuf(s, n);
            s += ct;
            n -= ct;

            if( m_state.m_wbuf.size() >= m_state.m_wbuf.capacity()){
                update_pptrs();
                overflow( traits_type::eof() );
            }

            //If buffer failed to flush, then top it off and finish
            if(m_state.m_wbuf.size())
                return ct + fill_wbuf(s, n);            
        }

        //Write buffer should be empty here

        //If the buffer is going to overflow anyway, write directly to the file
        if( n >= m_state.m_wbuf.capacity()){
            ct2 = m_state.m_file->write(s, n);
            s += ct2;
            n -= ct2;
        }

        //Now top off the buffer with any excess and finish        
        return ct + ct2 + fill_wbuf(s, n);        
    }

    //TODO: Handle no-op eof character
    virtual int_type overflow (int_type c) override{

		::ssize_t ct;
        PutGuard pg(*this);

		auto avail = m_state.m_wbuf.getavail();
	
		if(avail){
        	ct = m_state.m_file->write( m_state.m_wbuf.get_begin(), m_state.m_wbuf.getavail() );
			if(ct == 0)
				return traits_type::eof();
			else
				m_state.m_wbuf.pop(ct);
		}
        
		if( c != traits_type::eof() ){
        	*m_state.m_wbuf.put_begin() = c;
        	m_state.m_wbuf.push(1); //Not supposed to advance the position
		}

		return traits_type::to_int_type(c);
    }

    virtual int_type underflow() override{
        GetGuard gg(*this);
        auto space = m_state.m_rbuf.putavail();
        auto ct = m_state.m_file->read( m_state.m_rbuf.put_begin(), space );
        if(ct == 0)
            return traits_type::eof();
        
        m_state.m_rbuf.push(ct);
        return *m_state.
		m_rbuf.get_begin();
    }
};

template< typename Ch, class File, typename Traits = std::char_traits<Ch> >
class File_iostream : public std::basic_iostream<Ch, Traits>{
	using base_type = std::basic_iostream<Ch, Traits>;

public:
	using file_type = File;
	using char_type = base_type::char_type;
	using traits_type = base_type::traits_type;

private:
	std::unique_ptr<char_type []> m_bufspace;
	file_type m_file;
	Filestreambuf<char_type, traits_type> m_sb;	

public:
	File_iostream():		
		base_type(nullptr){
	}

	File_iostream(file_type &&file, std::streamsize sz = jab::util::Config::io_block_size):
		base_type(nullptr),
		m_bufspace( new char_type[sz * 2] ),
		m_file( std::move(file) ),
		m_sb( file ){

		m_sb.setbuf( m_bufspace.get(), sz * 2 );
		this->rdbuf(&m_sb);
	}
};

}
}
