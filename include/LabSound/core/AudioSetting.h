
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2015, The LabSound Authors. All rights reserved.

#ifndef AudioSetting_h
#define AudioSetting_h

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lab
{

// value defaults to zero, assumed set as integer.
// floatAssigned() is provided so that a user interface or RPC binding can configure
// automatically for float or unsigned integer values.
//
// a value changed callback is provided so that if tools set values, they can
// be responded to.
//
class AudioSetting
{
public:
    enum class Type
    {
        Bool,
        Integer,
        Float,
        Enumeration
    };

private:
    std::string _name;
    float _valf = 0;
    uint32_t _vali = 0;
    bool _valb = false;
    std::function<void()> _valueChanged;
    Type _type;
    char const * const * _enums = nullptr;

public:
    explicit AudioSetting(const std::string & n, Type t)
        : _name(n)
        , _type(t)
    {
    }
    explicit AudioSetting(char const * const n, Type t)
        : _name(n)
        , _type(t)
    {
    }
    explicit AudioSetting(const std::string & n, char const*const* enums)
        : _name(n)
        , _type(Type::Enumeration)
        , _enums(enums)
    {
    }
    explicit AudioSetting(char const * const n, char const*const* enums)
        : _name(n)
        , _type(Type::Enumeration)
        , _enums(enums)
    {
    }

    std::string name() const { return _name; }
    Type type() const { return _type; }
    char const*const* enums() const { return _enums; }

    bool valueBool() const { return _valb; }
    float valueFloat() const { return _valf; }
    uint32_t valueUint32() const { return _vali; }

    void setBool(bool v, bool notify = true)
    {
        if (v == _valb)
            return;

        _valb = v;
        if (notify && _valueChanged)
            _valueChanged();
    }

    void setFloat(float v, bool notify = true)
    {
        if (v == _valf)
            return;

        _valf = v;
        if (notify && _valueChanged)
            _valueChanged();
    }

    void setUint32(uint32_t v, bool notify = true)
    {
        if (v == _vali)
            return;

        _vali = v;
        if (notify && _valueChanged)
            _valueChanged();
    }

    void setEnumeration(int v, bool notify = true)
    {
        if (v == _vali)
            return;

        _vali = static_cast<int>(v);
        if (notify && _valueChanged)
            _valueChanged();
    }

    void setValueChanged(std::function<void()> fn) { _valueChanged = fn; }
};

}  // lab

#endif
