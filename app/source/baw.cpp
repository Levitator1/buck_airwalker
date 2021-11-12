#include <regex>

#include "util.hpp"
#include "concurrency/thread_pool.hpp"
#include "state_file.hpp"
#include "console.hpp"
#include "baw.hpp"
#include "packet_radio.hpp"
#include "File.hpp"
#include "Socket.hpp"
#include "exception.hpp"
#include "string.hpp"

using namespace std;
using namespace k3yab;
using namespace k3yab::bawns;
using namespace levitator::concurrency;
using namespace jab::util;
using namespace jab::file;
using namespace jab::exception;

baw_exception::baw_exception(const string &msg):
    runtime_error(msg){}

k3yab::bawns::baw::baw(const k3yab::bawns::Config &conf):
	m_config(conf){		
}

const bawns::Config &k3yab::bawns::baw::config() const{
	return m_config;
}

//unique_ptr which forwards the call operator
class callable_task_ptr : public std::unique_ptr<node_task>{
	using base_type = std::unique_ptr<node_task>;

public:
	using base_type::base_type;

	int operator()(){
		return (*this->get())();
	}
};

k3yab::bawns::node_task::node_task(baw &app, const std::string &callsign):
	m_appp(&app),
	m_callsign(callsign){}

k3yab::bawns::node_task::node_task():
	m_appp(nullptr){}

Console::out_type k3yab::bawns::node_task::print() const{	
	return console.out() << m_callsign << ": ";
}

//Discard stream data until a timeout happens
void k3yab::bawns::node_task::eat_stream( std::istream &stream ){
	string buf;

	try{
		while(true){		
			stream >> buf;		
			if(!stream)
				break;
			//print() << "test: " << buf << endl;
		}
		stream.clear( std::ios_base::goodbit );
	}
	catch( const std::ios::failure &fail ){
		if( jab::file::eof_exception::test(stream) ){
			stream.clear( stream.rdstate() & ~stream.eofbit );
			return;
		}
		else
			throw;		
	}
}

//Get a line allowing for four possbile line endings: \n, \r, \r\n, EOF
//Unlike the usual getline, does not include the line ending in the result.
//It's confusing, and what for? You know the result ends with the line.
static std::string my_getline( std::istream &stream, std::string &result ){

	using traits = std::istream::traits_type;	

	//Without eofbit, eof will cause empty strings to return
	//stream.exceptions( std::ios_base::eofbit );

	try{		
		while(stream){
			auto c = stream.get();
			if(c == traits::eof())
				break;
			else if ( c == '\n')
				break;
			else if(c == '\r'){
				c = stream.get();
				if(c == '\n')
					break;
				else{
					stream.putback( traits::char_type(c) );
					break;
				}
			}
			else
				result.push_back(c);
		}
	}
	catch( const std::ios_base::failure &phail ){		
		jab::file::eof_exception::check(stream); 	//throw an eof-specific exception if applicable
		throw; 										//otherwise just throw	
	}

	return result;
} 

//Allow for calllsigns with a star prefix, which I believe are Netrom aliases.
//Filter out "callsigns" with a slash, as these are usually the year from a last-seen datestamp.
const std::regex k3yab::bawns::node_task::callsign_regex{   "(?:.*?)(\\*?\\b[a-zA-Z0-9]{3,8})(-[0-9]{1,2})?\\b(?:.*?)" };

void k3yab::bawns::baw::send_command( std::ostream &stream, const std::string &cmd ){
	stream << cmd << "\r" << std::endl << std::flush;
}

std::string k3yab::bawns::node_task::parse_callsign(std::string &str) const{
	
	std::smatch match;
	//Pull the next discernable callsign out of the line of text
	while(std::regex_search( str, match, callsign_regex)){
		
		if(match.size() < 2)
			continue;				

		//If this match says "VIA", it just means the upcoming list of nodes is the route
		//If it has a slash it's probably a date, not a callsign

		{
			//Stupid fun with guard objects. This actually does make the code simpler and more readable
			//so maybe I should do it more. It means; on exiting the if statement, either way, do this.
			//Could be even more useful for complicated switch() statements with lots of different returns or exits
			auto advance = Guard( [&](){ str = match.suffix(); } );	

			if( match[0].str().find('/') == std::string::npos ){			
				return match[1].str() + (match.size() >= 3 ? match[2].str() : std::string());
			}
		}
	}
	return {};
}

static bool is_bbs_prompt( const std::string &line ){
	return line.ends_with("> ") || line.ends_with(">");
}

//attempt to put the remote host into BBS mode which offers various seemingly conventional
//if not standard services.
bool k3yab::bawns::node_task::bbs_mode( std::iostream &stream ){
	//Send the typical BBS-mode command, which is "BBS". Some hosts will already be in BBS mode
	//and that will probably return an error (or a carriage return) we won't understand and result in a timeout and false failure.
	baw::send_command(stream, "BBS");

	try{
		//Keep fetching lines of reply text until we find a BBS command prompt, concluding success,
		//Or time out, returning false for failure.
		std::string line;
		while( stream ){
			line.clear();			
			my_getline(stream, line);
			if( is_bbs_prompt(line) )
				return true;
		}
		return false;
	}
	catch( const jab::file::eof_exception & ){
		return false;
	}
}

//
// The J L command on some BBSes will display a long-form list of contacts with routing and timestamps
//
k3yab::bawns::node_task::route_result_type k3yab::bawns::node_task::try_j_l_command( std::iostream &stream){

	//constexpr bool c_debug = false;
	constexpr bool c_debug = true;

	std::vector<std::string> route;
	std::string current_node, forward_node;
	std::string line, cs;
	//stream << "J L\r" << endl << flush;

	stream.exceptions( stream.goodbit );
	stream.clear( stream.goodbit );
	baw::send_command(stream, "J L");
	
	while(stream){		

		//We'll use the emptiness of the string buffer as a flag
		//to decide whether to resume a partial node or pull in more data
		if(!line.length()){
			my_getline(stream, line);

			if(c_debug)
				print() << "Node line: " << line << std::endl;

			//If the line looks like a command prompt, then the query is done
			if(is_bbs_prompt(line)){				
				print() << "This previous line looks like a command prompt, so route scan is done." << std::endl;
				break;
			}

			//Get the first callsign, which is probably the destination node
			cs = parse_callsign(line);
		}
		
		if(!cs.length()){
			line.clear();
			continue;
		}

		current_node = cs;
		print() << "Fetching node " << cs << "... " << std::endl;
		//auto do_endl = Guard( [&](){ pr << std::endl; } );

		//See if there's a destination callsign to forward to. Blank is presumably destined for same node.
		forward_node = parse_callsign(line);

		//Make sure there aren't more callsigns on the line as that's not what we'll expect
		cs = parse_callsign(line);
		if(cs.length()){
			line.clear();
			print() << "Got more than two callsigns (" << cs << ") on the initial line of text from remote." << std::endl <<
				"So, we will give up on this host since we don't understand it.";
			break;
		}

		//Fetch the next line which should either start with "VIA", or represent the next node to process
		line.clear();
		my_getline(stream, line);
		if(c_debug)
				print() << "Node line: " << line << std::endl;
		if(is_bbs_prompt(line)){				
			print() << "This previous line looks like a command prompt, so route scan is done." << std::endl;
			break;
		}

		//Peek at the next callsign, which may be "VIA"
		cs = parse_callsign(line);
		if(!cs.length()){
			line.clear();
			continue;
		}
		
		if(jab::util::toupper(cs) != "VIA"){
			continue;
		}

		auto pr = print() << "'" << current_node << "' route: ";
		//This is the via list, representing the route
		while( (cs = parse_callsign(line)).size() ){
			route.push_back(cs);
			pr << cs << ", ";
		}
		route.push_back(current_node);
		pr << "\b" << std::endl;		
	}

	return {route, forward_node};
}

int k3yab::bawns::node_task::run(){
	if(!m_appp)
		return -1;	
	
	print() << " connecting..." << endl;
	
	Socket sock(AF_AX25, SOCK_SEQPACKET, 0);
	sock.timeout_as_eof(true);
	sock.rx_timeout( m_appp->config().response_timeout );
	AX25SockAddr local( m_appp->config().local_address );
	AX25SockAddr addr( m_callsign );

	sock.bind( local, sizeof(local) );
	sock.connect( addr, sizeof(addr) );
	print() << "CONNECTED" << endl;

	File_iostream<char, Socket> stream( std::move(sock) );

	//stream.exceptions( std::ios_base::badbit );
	stream.exceptions( stream.eofbit | stream.badbit );

	eat_stream(stream);
	auto bbsm = bbs_mode(stream);

	print() << (bbsm ? "BBS mode entered successfully" : "Failed getting into BBS mode, may cause failures") << std::endl;

	try_j_l_command(stream);
	return 0;
}

int k3yab::bawns::node_task::operator()(){

	try{
		run();
		print() << "COMPLETE" << endl;
	}
	catch( const std::exception &ex ){
		print() << "Abandoning this node with errors..." << endl << ex << endl;
	}

	return 0;
}

void k3yab::bawns::baw::run(){
	using thread_pool_type = ThreadPool< node_task  >;

	console.out() << "Starting..." << endl;
	thread_pool_type workers(m_config.threads);
	console.out() << "Using local callsign: " << m_config.local_address << endl;
	console.out() << "Using state file: " << m_config.state_path << endl;
	m_state = { m_config.state_path };
	//auto pending_count = m_state.pending.size();	
	//console.out() << "Pending or incomplete nodes from a previous run: " << pending_count << endl;
	console.out() << "Total nodes known: " << m_state.size() << endl;

	//TODO: Previous state resumption goes here (incomplete nodes, etc.)

	console.out() << "Reading stdin for root node callsigns, one per line..." << endl;

	string call;
	int ct = 0;
	while(console.in()){
		std::getline(console.in().get_istream(), call);

		//eof will count as a blank line!
		if(call.size() <= 1)
			continue;

		//Remove the trailing newline
		if( call.back() == '\n'){
			if(call.ends_with("\r\n"))
				call.resize( call.size() - 2 );
			else
				call.resize( call.size() - 1 );
		}

		if(call.size() <= 1)
			continue;

		workers.push( { *this, call } );
		++ct;
	}

	console.out() << ct << " callsigns read. Running query threads..." << endl;
	workers.shutdown();
}
