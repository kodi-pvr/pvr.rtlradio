//-----------------------------------------------------------------------------
// Copyright (c) 2020-2022 Michael G. Brehm
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef __LIBUSB_EXCEPTION_H_
#define __LIBUSB_EXCEPTION_H_
#pragma once

#include <exception>
#include <string>

#pragma warning(push, 4)

//-----------------------------------------------------------------------------
// Class libusb_exception
//
// std::exception used to wrap libusb error conditions

class libusb_exception : public std::exception
{
public:
  // Instance Constructors
  //
  libusb_exception(int code);

  // Copy Constructor
  //
  libusb_exception(libusb_exception const& rhs);

  // Move Constructor
  //
  libusb_exception(libusb_exception&& rhs);

  // char const* conversion operator
  //
  operator char const*() const;

  //-------------------------------------------------------------------------
  // Member Functions

  // what (std::exception)
  //
  // Gets a pointer to the exception message text
  virtual char const* what(void) const noexcept override;

private:
  //-------------------------------------------------------------------------
  // Member Variables

  std::string m_what; // libusb error message
};

//-----------------------------------------------------------------------------

#pragma warning(pop)

#endif // __LIBUSB_EXCEPTION_H_
