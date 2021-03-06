// MIT license
// 
// Copyright 2017 Vyacheslav Napadovsky <napadovskiy@gmail.com>.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#pragma once

#ifndef LIBUDFREAD_API
#	ifdef LIBUDFREAD_EXPORTS
#		define LIBUDFREAD_API __declspec(dllexport)
#	else
#		define LIBUDFREAD_API __declspec(dllimport)
#	endif
#endif

#include <streambuf>

struct udfread;
typedef struct udfread_file UDFFILE;

class LIBUDFREAD_API udffilebuf : public std::streambuf {
public:
	udffilebuf(const char* filename);
	~udffilebuf();
	std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which) override;
	std::streampos seekpos(std::streampos sp, std::ios_base::openmode which) override;
	int underflow() override;

	udffilebuf(const udffilebuf& other) = delete;
	udffilebuf& operator=(const udffilebuf& other) = delete;
private:
	udfread *_udf;
	udfread_file *_file;
	std::streamoff _size;
	char *_buffer;

	std::streamoff getpos();
};
