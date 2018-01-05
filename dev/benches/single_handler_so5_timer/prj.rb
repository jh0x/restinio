require 'mxx_ru/cpp'
MxxRu::Cpp::exe_target {

	if ENV.has_key?("RESTINIO_USE_BOOST_ASIO")
		# Add boost libs:
		if ENV["RESTINIO_USE_BOOST_ASIO"] == "shared"
			lib_shared( 'boost_system' )
		else
			lib_static( 'boost_system' )
		end
	else
		required_prj 'asio_mxxru/prj.rb'
	end


	required_prj 'nodejs/http_parser_mxxru/prj.rb'
	required_prj 'fmt_mxxru/prj.rb'
	required_prj 'restinio/platform_specific_libs.rb'
	required_prj 'so_5/prj.rb'

	target( "_bench.restinio.single_handler_so5_timer" )

	cpp_source( "main.cpp" )
}

