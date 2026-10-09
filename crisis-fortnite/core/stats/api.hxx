#pragma once

namespace stats::api {
	struct request_t {
		std::wstring host;
		std::wstring path;
		bool secure { true };
		std::vector< std::pair< std::wstring, std::wstring > > headers;
	};

	struct response_t {
		bool success { false };
		std::uint32_t status_code { };
		std::string body;
	};

	[[nodiscard]] response_t get( const request_t& request );
	[[nodiscard]] std::string url_encode( const std::string& value );
	[[nodiscard]] std::wstring to_wide( const std::string& value );
	[[nodiscard]] response_t fetch_asset( const std::wstring& path );
	[[nodiscard]] std::string to_lower( std::string value );
	[[nodiscard]] std::optional< std::string > extract_string_field(
		const std::string& json, const std::string& key );
	[[nodiscard]] std::optional< double > extract_number_field(
		const std::string& json, const std::string& key );
	[[nodiscard]] std::string slice_object(
		const std::string& json, const std::string& key, std::size_t from = 0 );
	[[nodiscard]] std::optional< std::string > extract_between(
		const std::string& text, const std::string& start_marker,
		const std::string& end_marker, std::size_t from = 0 );
	[[nodiscard]] std::string strip_tags( const std::string& fragment );
}
