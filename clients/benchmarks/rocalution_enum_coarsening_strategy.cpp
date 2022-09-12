/*! \file */
/* ************************************************************************
* Copyright (C) 2022 Advanced Micro Devices, Inc. All rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
* ************************************************************************ */
#include "rocalution_enum_coarsening_strategy.hpp"

constexpr const char*
    rocalution_enum_coarsening_strategy::names[rocalution_enum_coarsening_strategy::size];
constexpr rocalution_enum_coarsening_strategy::value_type
    rocalution_enum_coarsening_strategy::all[rocalution_enum_coarsening_strategy::size];

#include <iostream>
bool rocalution_enum_coarsening_strategy::is_invalid() const
{
    for(auto v : all)
    {
        if(this->value == v)
        {
            return false;
        }
    }
    return true;
}

rocalution_enum_coarsening_strategy::rocalution_enum_coarsening_strategy(const char* name_)
{
    for(auto v : all)
    {
        if(!strcmp(name_, names[v]))
        {
            this->value = v;
            return;
        }
    }

    rocalution_bench_errmsg << "coarsening_strategy '" << name_
                            << "' is invalid, the list of valid coarsening_strategys is"
                            << std::endl;
    for(auto v : all)
    {
        rocalution_bench_errmsg << "   - '" << names[v] << "'" << std::endl;
    }
    throw false;
};

//
// Default contructor.
//
rocalution_enum_coarsening_strategy::rocalution_enum_coarsening_strategy()
    : value((value_type)-1){};

//
//
//
rocalution_enum_coarsening_strategy&
    rocalution_enum_coarsening_strategy::operator()(const char* name_)
{
    this->value = (value_type)-1;
    for(auto v : all)
    {
        if(!strcmp(name_, names[v]))
        {
            this->value = v;
            return *this;
        }
    }

    rocalution_bench_errmsg << "coarsening_strategy '" << name_
                            << "' is invalid, the list of valid coarsening_strategys is"
                            << std::endl;
    for(auto v : all)
    {
        rocalution_bench_errmsg << "   - '" << names[v] << "'" << std::endl;
    }

    throw false;
}