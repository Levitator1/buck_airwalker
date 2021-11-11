#pragma once
#include <sys/ioctl.h>
#include <utility>
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
	File( const File & );
    File &operator=(const File &) = delete;
    
    //Fundamental I/O operations
    virtual void close();
    virtual std::streamsize read(char *data, std::streamsize len);
    virtual std::streamsize write(const char *data, std::streamsize len);
	virtual std::streamsize seek(std::streamsize pos, std::ios_base::seekdir dir);
	virtual std::streamsize seek_exactly(std::streamsize pos, std::ios_base::seekdir dir);
    virtual std::streamsize available() const;
    virtual void flush();
    
    operator bool() const;
    operator fd_t() const;
	fd_t fd() const;
	std::streamsize tell() const;
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

// First of all the C++ streambuf design sucks. It's supposed to be extendable, and it is,
// but it takes a ridiculous amount of work for something that boils down to a variation of read()/write()
// on every operating system I've ever used.
//
// Anyway, this design seems to assume that the file maintains separate read()/write() pointers. Unices 
// have a single lseek() function which seems to position the file for both R/W, so to have separate pointers
// you have to seek the file (within streambuf) every time you switch from reading to writing or vice versa. Some file types
// don't support seeking. So, clearly, in order to be compatible with more general UNIX-style file abstractions, something
// has to give, so we stipulate that all seeks must be R/W rather than single-sided. This way, you can choose not to seek at all
// and everything works, including non-seekable files. Or, you can seek both read and write pointers at the same time,
// and that will also work because it's supported by the OS, without presupposing that the file is seekable and having
// to seek each time you switch R/W or W/R (because the two pointers are always the same).
//
template< typename Char = char, class Traits = std::char_traits<Char> >
class Filestreambuf : public std::basic_streambuf<Char, Traits> {

    using base_type = std::basic_streambuf<Char, Traits>;
    using char_type = Char;    
    using pos_type = typename base_type::pos_type;
    using off_type = typename base_type::off_type;
    using traits_type = typename base_type::traits_type;    
    using int_type = typename base_type::int_type;
    using streamsize = std::streamsize;

	struct State{
		char_type rbuf_bak, wbuf_bak; //single-character fallback buffers in case none are provided
		jab::io::ringbuffer<char_type> rbuf, wbuf;
		File *file;				
	} m_state;

    //Synchronize the streambuf get-pointers with the get-ringbuffer
    void update_gptrs(){
		//If the buffer is more than 1-sized, then reserve a character as a reliable put-back space
		auto end = m_state.rbuf.size() > 1 ? m_state.rbuf.get_end() - 1 : m_state.rbuf.get_end();
        this->setg(m_state.rbuf.get_begin(), m_state.rbuf.get_begin(), end);
    }

    //Synchronize the streambuf put-pointers with the put-ringbuffer
    void update_pptrs(){
        this->setp( m_state.wbuf.put_begin(), m_state.wbuf.put_end());
    }

    //Commit writes in the buffer window to the ringbuffer
    void commit_wbuf(){
        m_state.wbuf.push( this->pptr() - this->pbase() );
    }

    //Commit reads in the read-buffer window to the ringbuffer
    void commit_rbuf(){
        m_state.rbuf.pop( this->gptr() - this->eback() );
    }
    
    //These guards commit updates to the user-facing end of the ringbuffer
    //And when they go away, they ensure that the corresponding streambuf pointers are synchronized to the ringbuffer
    class GetGuard{
        Filestreambuf &m_buf;
    public:
        GetGuard( Filestreambuf &buf ):
            m_buf(buf){
                m_buf.commit_rbuf();
				m_buf.update_pptrs();
        }

        ~GetGuard(){
            m_buf.update_gptrs();
        }
    };

	//* Ok, it's kind of ridiculous to keep track of what to update when, so rather than do that
	//Let's just make sure that the guards always leave the streambuf pointers updated, even following init
	//and that allows the guards to be used recursively because they won't encounter an inconsistent state
    class PutGuard{
        Filestreambuf &m_buf;
    public:
        PutGuard( Filestreambuf &buf ):
            m_buf(buf){
                m_buf.commit_wbuf();
				m_buf.update_pptrs(); //*
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

	void file( File &file ){
		m_state.file = &file;
	}

	Filestreambuf &operator=(Filestreambuf &&rhs){
		m_state = std::move(rhs.m_state);
		rhs.m_state.file = &null_file;
		return *this;
	}

    //streamsize getbufavail() const{
        //return this->egptr() - this->gptr();
    //}

    //TODO: Attempt to presrve the previous buffer data, because right now, this will purge it
    virtual base_type* setbuf (char* s, std::streamsize n) override{
        auto half = n / 2;
        if( half < 1 ){            
            m_state.rbuf.buf(&m_state.rbuf_bak, 1);
            m_state.wbuf.buf(&m_state.wbuf_bak, 1);            
        }
        else{
            m_state.rbuf.buf(s, half);
            m_state.wbuf.buf(&s[half], half);               
        }   

        update_gptrs();
        update_pptrs();
        return this;
    }

    virtual pos_type seekpos (pos_type pos, std::ios_base::openmode which) override{
		return seekoff(pos, std::ios_base::beg, which);
    }

    virtual pos_type seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which) override{

		if( !(which & std::ios_base::in && which & std::ios_base::out) )
			throw std::invalid_argument("One-sided seeking is not supported. Must seek in/out simultaneously.");

		//For simplicity just flush and purge the buffers
		//TODO: could be optimized to retain some buffer data when seeking short distances
		m_state.rbuf.clear();
		update_gptrs();
		drain_write_buffer();
		return m_state.file->seek(off, way);
    }

private:
    streamsize drain_getbuf( char_type *s, streamsize n ){
        streamsize ct = std::min((::size_t)n, m_state.rbuf.getavail() );
        std::copy( m_state.rbuf.get_begin(), m_state.rbuf.get_begin() + ct, s );
        m_state.rbuf.pop(ct);
        return ct;
    }

	//Drain the write buffer until it's either empty or EOF
	//return the count of characters written
	//streambuf put-pointers must be current on entry (noted dbefore PutGuard made them current)
	streamsize drain_write_buffer(){
		streamsize sz, ct = 0, tot=0;

		PutGuard guard(*this);
		while( (sz = m_state.wbuf.size()) ){
			overflow( traits_type::eof() );
			ct = sz - m_state.wbuf.size();			
			if(ct == 0)
				break;
			else
				tot+=ct;
		}
		return tot;
	}

public:
    //These next two only pertain to the underlying file and do not affect the buffering state according to the crappy online references.
	//However, that seems to leave stuff in the buffer when the file closes. So we will flush the streambuf buffer as well.
    virtual int sync() override{        
        try{			
			drain_write_buffer();
            m_state.file->flush();
            return 0;
        }
        catch(...){
            return -1;
        }
    }

    virtual streamsize showmanyc() override{
        return m_state.file->available();
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
            if( n >= m_state.rbuf.capacity() ){
				ct = do_read(s, n);	
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

        if(m_state.rbuf.size() >= m_state.rbuf.capacity())
            return traits_type::eof();

        m_state.rbuf.unpop();
        *m_state.rbuf.get_begin() = c;        
        return c;
    }

private:
    streamsize fill_wbuf(const char_type* s, streamsize n){        
        n = std::min( m_state.wbuf.putavail(), (::size_t)n);
        std::copy( s, s+n, m_state.wbuf.put_begin() );
        m_state.wbuf.push(n);
        return n;
    }

	streamsize do_write(const char_type *buf, streamsize n){

		//Now, it's not possible for a write to occur to any position buffered in the get-buffer
		//because with a single file pointer, we are writing after the last position we accessed or sought.
		//If a seek were to occurr to a previously read position, we would have purged the get buffer
		//as we do on all seeks, so there is nothing there to update on subsequent write.
		return m_state.file->write(buf, n);
	}

	streamsize do_read(char_type *buf, streamsize n){
		return m_state.file->read(buf, n);
	}

public:
    virtual streamsize xsputn(const char_type* s, streamsize n) override{

        streamsize ct=0, ct2=0;

        PutGuard pg(*this);

        //If the buffer is already partially populated than try to fill it
        if( m_state.wbuf.size() ){
            ct = fill_wbuf(s, n);
            s += ct;
            n -= ct;

            if( m_state.wbuf.size() >= m_state.wbuf.capacity()){
                update_pptrs();
                overflow( traits_type::eof() );
            }

            //If buffer failed to flush, then top it off and finish
            if(m_state.wbuf.size())
                return ct + fill_wbuf(s, n);            
        }

        //Write buffer should be empty here

        //If the buffer is going to overflow anyway, write directly to the file
        if( n >= m_state.wbuf.capacity()){
			ct2 = do_write( s, n );						
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

		auto avail = m_state.wbuf.getavail();
	
		if(avail){
			ct = do_write( m_state.wbuf.get_begin(), m_state.wbuf.getavail() );			
			if(ct == 0)
				return traits_type::eof();
			else{				
				m_state.wbuf.pop(ct);
			}
		}
        
		if( c != traits_type::eof() ){
        	*m_state.wbuf.put_begin() = c;
        	m_state.wbuf.push(1); //Not supposed to advance the position
		}

		return traits_type::to_int_type(c);
    }

    virtual int_type underflow() override{
        GetGuard gg(*this);
        auto space = m_state.rbuf.putavail();
		auto ct = do_read(m_state.rbuf.put_begin(), space);

        if(ct == 0)
            return traits_type::eof();

        m_state.rbuf.push(ct);
        return traits_type::to_int_type(*m_state.rbuf.get_begin());
    }
};

template< typename Ch, class File, typename Traits = std::char_traits<Ch> >
class File_iostream : public std::basic_iostream<Ch, Traits>{
	using base_type = std::basic_iostream<Ch, Traits>;

public:
	using file_type = File;
	using char_type = typename base_type::char_type;
	using traits_type = typename base_type::traits_type;

private:
	struct State{
		std::unique_ptr<char_type []> m_bufspace;
		file_type file;
		Filestreambuf<char_type, traits_type> m_sb;
		
		State( char_type *buf = nullptr, file_type &&file_arg = file_type()):
			m_bufspace(buf),
			file(std::move(file_arg)),
			m_sb(file){}

	} m_state;

public:
	File_iostream():		
		base_type(nullptr){
	}

	File_iostream(file_type &&file, std::streamsize sz = jab::util::Config::io_block_size):
		base_type(nullptr),
		m_state( new char_type[sz * 2], std::move(file) ) {

		m_state.m_sb.setbuf( m_state.m_bufspace.get(), sz * 2 );
		this->rdbuf(&m_state.m_sb);
	}

	File_iostream &operator=( File_iostream &&rhs ){
		m_state = std::move(rhs.m_state);
		m_state.m_sb.file(m_state.file);
		this->rdbuf(&m_state.m_sb);
		return *this;
	}

	const file_type &file() const{
		return m_state.file;
	}

	file_type &file(){
		return m_state.file;
	}
};

}
}
