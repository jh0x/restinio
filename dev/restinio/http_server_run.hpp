/*
 * restinio
 */

/*!
 * \file
 * \brief Helper function for simple run of HTTP server.
 */

#pragma once

#include <restinio/impl/ioctx_on_thread_pool.hpp>

#include <restinio/http_server.hpp>

namespace restinio
{

//
// break_signal_handling_t
//
//FIXME: document this!
enum class break_signal_handling_t
{
	used,
	skipped
};

//FIXME: document this!
inline constexpr break_signal_handling_t
use_break_signal_handling() noexcept
{
	return break_signal_handling_t::used;
}

inline constexpr break_signal_handling_t
skip_break_signal_handling() noexcept
{
	return break_signal_handling_t::skipped;
}

//
// run_on_this_thread_settings_t
//
/*!
 * \brief Settings for the case when http_server must be run
 * on the context of the current thread.
 *
 * \note
 * Shouldn't be used directly. Only as result of on_this_thread()
 * function as parameter for run().
 */
template<typename Traits>
class run_on_this_thread_settings_t final
	:	public basic_server_settings_t<
				run_on_this_thread_settings_t<Traits>,
				Traits>
{
	using base_type_t = basic_server_settings_t<
				run_on_this_thread_settings_t<Traits>, Traits>;
public:
	// Inherit constructors from base class.
	using base_type_t::base_type_t;
};

//
// on_this_thread
//
/*!
 * \brief A special marker for the case when http_server must be
 * run on the context of the current thread.
 *
 * Usage example:
 * \code
 * // Run with the default traits.
 * run( restinio::on_this_thread()
 * 	.port(8080)
 * 	.address("localhost")
 * 	.request_handler(...) );
 * \endcode
 * For a case when some custom traits must be used:
 * \code
 * run( restinio::on_this_thread<my_server_traits_t>()
 * 	.port(8080)
 * 	.address("localhost")
 * 	.request_handler(...) );
 * \endcode
 */
template<typename Traits = default_single_thread_traits_t>
run_on_this_thread_settings_t<Traits>
on_this_thread() { return run_on_this_thread_settings_t<Traits>{}; }

//
// run_on_thread_pool_settings_t
//
/*!
 * \brief Settings for the case when http_server must be run
 * on the context of the current thread.
 *
 * \note
 * Shouldn't be used directly. Only as result of on_thread_pool()
 * function as parameter for run().
 */
template<typename Traits>
class run_on_thread_pool_settings_t final
	:	public basic_server_settings_t<
				run_on_thread_pool_settings_t<Traits>,
				Traits>
{
	//! Size of the pool.
	std::size_t m_pool_size;

public:
	//! Constructor.
	run_on_thread_pool_settings_t(
		//! Size of the pool.
		std::size_t pool_size )
		:	m_pool_size(pool_size)
	{}

	//! Get the pool size.
	std::size_t
	pool_size() const { return m_pool_size; }
};

//
// on_thread_pool
//
/*!
 * \brief A special marker for the case when http_server must be
 * run on an thread pool.
 *
 * Usage example:
 * \code
 * // Run with the default traits.
 * run( restinio::on_thread_pool(16) // 16 -- is the pool size.
 * 	.port(8080)
 * 	.address("localhost")
 * 	.request_handler(...) );
 * \endcode
 * For a case when some custom traits must be used:
 * \code
 * run( restinio::on_thread_pool<my_server_traits_t>(16)
 * 	.port(8080)
 * 	.address("localhost")
 * 	.request_handler(...) );
 * \endcode
 */
template<typename Traits = default_traits_t>
run_on_thread_pool_settings_t<Traits>
on_thread_pool(
	//! Size of the pool.
	std::size_t pool_size )
{
	return run_on_thread_pool_settings_t<Traits>( pool_size );
}

//
// run()
//


//! Helper function for running http server until ctrl+c is hit.
/*!
 * Can be useful when RESTinio server should be run on the user's
 * own io_context instance.
 *
 * For example:
 * \code
 * asio::io_context iosvc;
 * ... // iosvc used by user.
 * restinio::run(iosvc,
 * 		restinio::on_this_thread<my_traits>()
 * 			.port(8080)
 * 			.address("localhost")
 * 			.request_handler([](auto req) {...}));
 * \endcode
 *
 * \since
 * v.0.4.2
 */
template<typename Traits>
inline void
run(
	//! Asio's io_context to be used.
	//! Note: this reference should remain valid until RESTinio server finished.
	asio_ns::io_context & ioctx,
	//! Settings for that server instance.
	run_on_this_thread_settings_t<Traits> && settings )
{
	using settings_t = run_on_this_thread_settings_t<Traits>;
	using server_t = http_server_t<Traits>;

	server_t server{
		restinio::external_io_context( ioctx ),
		std::forward<settings_t>(settings) };

	asio_ns::signal_set break_signals{ server.io_context(), SIGINT };
	break_signals.async_wait(
		[&]( const asio_ns::error_code & ec, int ){
			if( !ec )
			{
				server.close_async(
					[&]{
						// Stop running io_service.
						ioctx.stop();
					},
					[]( std::exception_ptr ex ){
						std::rethrow_exception( ex );
					} );
			}
		} );

	server.open_async(
		[]{ /* Ok. */},
		[]( std::exception_ptr ex ){
			std::rethrow_exception( ex );
		} );

	ioctx.run();
}

//! Helper function for running http server until ctrl+c is hit.
/*!
 * This function creates its own instance of Asio's io_context and
 * uses it exclusively.
 *
 * Usage example:
 * \code
 * restinio::run(
 * 		restinio::on_this_thread<my_traits>()
 * 			.port(8080)
 * 			.address("localhost")
 * 			.request_handler([](auto req) {...}));
 * \endcode
 */
template<typename Traits>
inline void
run(
	run_on_this_thread_settings_t<Traits> && settings )
{
	asio_ns::io_context io_context;
	run( io_context, std::move(settings) );
}

namespace impl {

/*!
 * \brief An implementation of run-function for thread pool case.
 *
 * This function receives an already created thread pool object and
 * creates and runs http-server on this thread pool.
 *
 * \since
 * v.0.4.2
 */
template<typename Io_Context_Holder, typename Traits>
void
run(
	ioctx_on_thread_pool_t<Io_Context_Holder> & pool,
	run_on_thread_pool_settings_t<Traits> && settings )
{
	using settings_t = run_on_thread_pool_settings_t<Traits>;
	using server_t = http_server_t<Traits>;

	server_t server{
		restinio::external_io_context( pool.io_context() ),
		std::forward<settings_t>(settings) };

	asio_ns::signal_set break_signals{ server.io_context(), SIGINT };
	break_signals.async_wait(
		[&]( const asio_ns::error_code & ec, int ){
			if( !ec )
			{
				server.close_async(
					[&]{
						// Stop running io_service.
						pool.stop();
					},
					[]( std::exception_ptr ex ){
						std::rethrow_exception( ex );
					} );
			}
		} );

	server.open_async(
		[]{ /* Ok. */},
		[]( std::exception_ptr ex ){
			std::rethrow_exception( ex );
		} );

	pool.start();
	pool.wait();
}

} /* namespace impl */

//! Helper function for running http server until ctrl+c is hit.
/*!
 * This function creates its own instance of Asio's io_context and
 * uses it exclusively.
 *
 * Usage example:
 * \code
 * restinio::run(
 * 		restinio::on_thread_pool<my_traits>(4)
 * 			.port(8080)
 * 			.address("localhost")
 * 			.request_handler([](auto req) {...}));
 * \endcode
 */
template<typename Traits>
inline void
run( run_on_thread_pool_settings_t<Traits> && settings )
{
	using thread_pool_t = impl::ioctx_on_thread_pool_t<
			impl::own_io_context_for_thread_pool_t >;

	thread_pool_t pool( settings.pool_size() );

	impl::run( pool, std::move(settings) );
}

//! Helper function for running http server until ctrl+c is hit.
/*!
 * Can be useful when RESTinio server should be run on the user's
 * own io_context instance.
 *
 * For example:
 * \code
 * asio::io_context iosvc;
 * ... // iosvc used by user.
 * restinio::run(iosvc,
 * 		restinio::on_thread_pool<my_traits>(4)
 * 			.port(8080)
 * 			.address("localhost")
 * 			.request_handler([](auto req) {...}));
 * \endcode
 *
 * \since
 * v.0.4.2
 */
template<typename Traits>
inline void
run(
	//! Asio's io_context to be used.
	//! Note: this reference should remain valid until RESTinio server finished.
	asio_ns::io_context & ioctx,
	//! Settings for that server instance.
	run_on_thread_pool_settings_t<Traits> && settings )
{
	using thread_pool_t = impl::ioctx_on_thread_pool_t<
			impl::external_io_context_for_thread_pool_t >;

	thread_pool_t pool{ settings.pool_size(), ioctx };

	impl::run( pool, std::move(settings) );
}

//FIXME: document this!
//
// run_existing_server_on_thread_pool_t
//
template<typename Traits>
class run_existing_server_on_thread_pool_t
{
	std::size_t m_pool_size;
	break_signal_handling_t m_break_handling;
	http_server_t<Traits> * m_server;

public:
	run_existing_server_on_thread_pool_t(
		std::size_t pool_size,
		break_signal_handling_t break_handling,
		http_server_t<Traits> & server )
		:	m_pool_size{ pool_size }
		,	m_break_handling{ break_handling }
		,	m_server{ &server }
	{}

	std::size_t
	pool_size() const noexcept { return m_pool_size; }

	break_signal_handling_t
	break_handling() const noexcept { return m_break_handling; }

	http_server_t<Traits> &
	server() const noexcept { return *m_server; }
};

//FIXME: document this!
template<typename Traits>
run_existing_server_on_thread_pool_t<Traits>
on_thread_pool(
	std::size_t pool_size,
	break_signal_handling_t break_handling,
	http_server_t<Traits> & server )
{
	return { pool_size, break_handling, server };
}

namespace impl {

/*!
 * \brief An implementation of run-function for thread pool case
 * with existing http_server instance.
 *
 * This function receives an already created thread pool object and
 * already created http-server and run it on this thread pool.
 *
 * \attention
 * This function installs break signal handler and stops server when
 * break signal is raised.
 *
 * \since
 * v.0.5.1
 */
template<typename Io_Context_Holder, typename Traits>
void
run_with_break_signal_handling(
	ioctx_on_thread_pool_t<Io_Context_Holder> & pool,
	http_server_t<Traits> & server )
{
	asio_ns::signal_set break_signals{ server.io_context(), SIGINT };
	break_signals.async_wait(
		[&]( const asio_ns::error_code & ec, int ){
			if( !ec )
			{
				server.close_async(
					[&]{
						// Stop running io_service.
						pool.stop();
					},
					[]( std::exception_ptr ex ){
						std::rethrow_exception( ex );
					} );
			}
		} );

	server.open_async(
		[]{ /* Ok. */},
		[]( std::exception_ptr ex ){
			std::rethrow_exception( ex );
		} );

	pool.start();
	pool.wait();
}

/*!
 * \brief An implementation of run-function for thread pool case
 * with existing http_server instance.
 *
 * This function receives an already created thread pool object and
 * already created http-server and run it on this thread pool.
 *
 * \note
 * This function doesn't install break signal handlers.
 *
 * \since
 * v.0.5.1
 */
template<typename Io_Context_Holder, typename Traits>
void
run_without_break_signal_handling(
	ioctx_on_thread_pool_t<Io_Context_Holder> & pool,
	http_server_t<Traits> & server )
{
	server.open_async(
		[]{ /* Ok. */},
		[]( std::exception_ptr ex ){
			std::rethrow_exception( ex );
		} );

	pool.start();
	pool.wait();
}

} /* namespace impl */

//FIXME: actualize docs!
//! Helper function for running http server until ctrl+c is hit.
/*!
 * This function creates its own instance of Asio's io_context and
 * uses it exclusively.
 *
 * Usage example:
 * \code
 * restinio::run(
 * 		restinio::on_thread_pool<my_traits>(4)
 * 			.port(8080)
 * 			.address("localhost")
 * 			.request_handler([](auto req) {...}));
 * \endcode
 */
template<typename Traits>
inline void
run( run_existing_server_on_thread_pool_t<Traits> && params )
{
	using thread_pool_t = impl::ioctx_on_thread_pool_t<
			impl::external_io_context_for_thread_pool_t >;

	thread_pool_t pool{ params.pool_size(), params.server().io_context() };

	if( break_signal_handling_t::used == params.break_handling() )
		impl::run_with_break_signal_handling( pool, params.server() );
	else
		impl::run_without_break_signal_handling( pool, params.server() );
}

//
// initiate_shutdown
//
//FIXME: document this!
template<typename Traits>
inline void
initiate_shutdown( http_server_t<Traits> & server )
{
	server.io_context().post( [&server] {
			server.close_sync();
			server.io_context().stop();
		} );
}

//
// on_pool_runner_t
//
//FIXME: document this!
template<typename Http_Server>
class on_pool_runner_t
{
	//! HTTP-server to be run.
	Http_Server & m_server;

	//! Thread pool for running the server.
	impl::ioctx_on_thread_pool_t< impl::external_io_context_for_thread_pool_t >
			m_pool;

public :
	on_pool_runner_t( const on_pool_runner_t & ) = delete;
	on_pool_runner_t( on_pool_runner_t && ) = delete;

	//! Initializing constructor.
	on_pool_runner_t(
		//! Size of thread pool.
		std::size_t pool_size,
		//! Server instance to be run.
		//! NOTE. This reference must be valid for all life-time
		//! of on_pool_runner instance.
		Http_Server & server )
		:	m_server{ server }
		,	m_pool{ pool_size, server.io_context() }
	{}

	//! Start the server.
	void
	start()
	{
		m_server.open_async(
			[]{ /* Ok. */},
			[]( std::exception_ptr ex ){
				std::rethrow_exception( ex );
			} );

		m_pool.start();
	}


	//! Is server started.
	bool
	started() const noexcept { return m_pool.started(); }

	//! Stop the server.
	void
	stop()
	{
		m_server.close_async(
			[this]{
				// Stop running io_service.
				m_pool.stop();
			},
			[]( std::exception_ptr ex ){
				std::rethrow_exception( ex );
			} );
	}

	//! Wait for full stop of the server.
	void
	wait() { m_pool.wait(); }
};

} /* namespace restinio */

