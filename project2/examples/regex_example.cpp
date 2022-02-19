/**
 * File: regex_example.cpp
 *
 * @author Sat Garcia
 *
 * Example program demonstrating C++ regular expressions, starting with a
 * regular expression that supports only a limited set of HTTP GET request
 * messages.
 */
#include <iostream>
#include <string>
#include <regex>

using std::string;
using std::cout;

int main() {
	/*
	 * The following regular expression is a starting point for a complete,
	 * valid HTTP GET request.
	 * It is missing the following:
	 *
	 * 1. Support for slashes in the URI (e.g. "/foo/index.html" won't match)
	 * 2. It only supports requests for HTTP version 1.0. Most browswers these
	 *    days request 1.1, but you should support requests for any number of the
	 *    form digit.digit (e.g. 1.0, 1.1, 2.0, 9.3).
	 * 3. It does not support header lines (e.g. "Host: home.sandiego.edu")
	 *    after the first line. Most browsers include many of these (e.g.
	 *    "Host: foo.com").
	 * 4. It doesn't allow for multiple spaces or tabs. For example,
	 *    "GET  /index.html HTTP/1.0\r\n\r\n" won't match because there are two spaces
	 *    between the "GET" and the "/index.html".
	 */
	std::regex http_request_regex("GET /([a-zA-Z0-9_\\-\\.]*) HTTP/1\\.0\r\n\r\n",
					std::regex_constants::ECMAScript);

	// This will hold information about a potential match
	std::smatch request_match;

	// Attempt to match a valid HTTP request
	string valid_request = "GET /index.html HTTP/1.0\r\n\r\n";

	// request_match returns true or false, depending on whether the string
	// matched the given regular expression or not.
	if (std::regex_match(valid_request, request_match, http_request_regex)) {
		cout << "Request 1 WAS a match.\n";

		// Prints out the first "capture group" of the match.
		// A capture group is a part of the regular expression surrounded by
		// parentheses.
		// In this example, the capture group is the URI, excluding the
		// leading "/".
		cout << "URI: " << request_match[1] << "\n";
	}
	else {
		cout << "Request 1 was NOT a match.\n";
	}

	// Attempt to match a invalid HTTP request.
	// The following string is invalid because it is missing the "HTTP/" part
	// at end of request line".
	string invalid_request = "GET /foo\r\n\r\n";

	if (std::regex_match(invalid_request, request_match, http_request_regex)) {
		cout << "Request 2 WAS a match.\n";
		cout << "URI: " << request_match[1] << "\n";
	}
	else {
		cout << "Request 2 was NOT a match.\n";
	}
}
