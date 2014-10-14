/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _NOIRO__TRICKS__OVERLOAD_H 
#define _NOIRO__TRICKS__OVERLOAD_H 

#define GET_MACRO(                                     fn \
        ,_01 ,_02 ,_03 ,_04 ,_05 ,_06 ,_07 ,_08 ,_09 ,_10 \
        ,_11 ,_12 ,_13 ,_14 ,_15 ,_16 ,_17 ,_18 ,_19 ,_20 \
        ,_21 ,_22 ,_23 ,_24 ,_25 ,_26 ,_27 ,_28 ,_29 ,_30 \
        ,_31 ,_32 ,_33 ,_34 ,_35 ,_36 ,_37 ,_38 ,_39 ,_40 \
        ,_41 ,_42 ,_43 ,_44 ,_45 ,_46 ,_47 ,_48 ,_49 ,_50 \
        ,_51 ,_52 ,_53 ,_54 ,_55 ,_56 ,_57 ,_58 ,_59 ,_60 \
        ,_61 ,_62                                         \
        ,NAME                                             \
        ,...) fn##NAME

#define VARIADIC_FN(fn, ...) GET_MACRO(fn,           \
        ##__VA_ARGS__,    _62args, _61args, _60args, \
        _59args, _58args, _57args, _56args, _55args, \
        _54args, _53args, _52args, _51args, _50args, \
        _49args, _48args, _47args, _46args, _45args, \
        _44args, _43args, _42args, _41args, _40args, \
        _39args, _38args, _37args, _36args, _35args, \
        _34args, _33args, _32args, _31args, _30args, \
        _29args, _28args, _27args, _26args, _25args, \
        _24args, _23args, _22args, _21args, _20args, \
        _19args, _18args, _17args, _16args, _15args, \
        _14args, _13args, _12args, _11args, _10args, \
        _09args, _08args, _07args, _06args, _05args, \
        _04args, _03args, _02args, _01args, _00args  \
        )(__VA_ARGS__)

#endif/*_NOIRO__TRICKS__OVERLOAD_H*/ 
