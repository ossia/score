#pragma once
/*
 Released under the MIT License - https://opensource.org/licenses/MIT

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <string_view>

namespace doj
{
struct alphanum_compare
{
  static constexpr bool alphanum_isdigit(const char c) noexcept
  {
    return c >= '0' && c <= '9';
  }
  /**
       compare l and r with strcmp() semantics, but using
       the "Alphanum Algorithm". This function is designed to read
       through the l and r strings only one time, for
       maximum performance. It does not allocate memory for
       substrings. It can either use the C-library functions isdigit()
       and atoi() to honour your locale settings, when recognizing
       digit characters when you "#define ALPHANUM_LOCALE=1" or use
       it's own digit character handling which only works with ASCII
       digit characters, but provides better performance.

       @param l NULL-terminated C-style string
       @param r NULL-terminated C-style string
       @return negative if l<r, 0 if l equals r, positive if l>r
     */
  static constexpr int impl(std::string_view ll, std::string_view rr) noexcept
  {
    enum mode_t
    {
      STRING,
      NUMBER
    } mode
        = STRING;

    const char* l = ll.data();
    const char* r = rr.data();
    while(l != ll.end() && r != rr.end() && *l && *r)
    {
      if(mode == STRING)
      {
        char l_char{}, r_char{};
        while((l != ll.end() && r != rr.end()) && (l_char = *l) && (r_char = *r))
        {
          // check if this are digit characters
          const bool l_digit = alphanum_isdigit(l_char),
                     r_digit = alphanum_isdigit(r_char);
          // if both characters are digits, we continue in NUMBER mode
          if(l_digit && r_digit)
          {
            mode = NUMBER;
            break;
          }
          // if only the left character is a digit, we have a result
          if(l_digit)
            return -1;
          // if only the right character is a digit, we have a result
          if(r_digit)
            return +1;
          // compute the difference of both characters
          const int diff = l_char - r_char;
          // if they differ we have a result
          if(diff != 0)
            return diff;
          // otherwise process the next characters
          ++l;
          ++r;
        }
      }
      else // mode==NUMBER
      {
        // get the left number
        unsigned long l_int = 0;
        while(l != ll.end() && *l && alphanum_isdigit(*l))
        {
          // TODO: this can overflow
          l_int = l_int * 10 + *l - '0';
          ++l;
        }

        // get the right number
        unsigned long r_int = 0;
        while(r != rr.end() && *r && alphanum_isdigit(*r))
        {
          // TODO: this can overflow
          r_int = r_int * 10 + *r - '0';
          ++r;
        }

        // if the difference is not equal to zero, we have a comparison result
        const long diff = l_int - r_int;
        if(diff != 0)
          return diff;

        // otherwise we process the next substring in STRING mode
        mode = STRING;
      }
    }

    if(*r)
      return -1;
    if(*l)
      return +1;
    return 0;
  }

  constexpr bool operator()(std::string_view l, std::string_view r) const noexcept
  {
    return impl(l.data(), r.data()) < 0;
  }
};
}
