/*
 * Copyright (c) 2020 Giso Grimm
 */
/*
 * ov-client is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * ov-client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with ov-client. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ERRMSG_H
#define ERRMSG_H

#include <string>

class ErrMsg : public std::exception, private std::string {
public:
  ErrMsg(const std::string& msg);
  ErrMsg(const std::string& msg, int err);
  virtual ~ErrMsg() throw();
  const char* what() const throw();
};

#endif

/*
 * Local Variables:
 * mode: c++
 * compile-command: "make -C .."
 * End:
 */
