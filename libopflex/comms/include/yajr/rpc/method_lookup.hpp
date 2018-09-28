/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

    /* Switch-case statement with compile time hash values.
     * => good compilers will turn this into a perfect hash of hashes...
     * Alas, this could be done much more elegantly with C++11.
     */
    switch (boost::hash_range(method, method + strlen(method))) {

      case meta::hash_string< boost::mpl::string<'echo'
                                                 > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::echo);
          break;

      case meta::hash_string< boost::mpl::string<'send',
                                                  '_ide',
                                                  'ntit',
                                                  'y'
                                                 > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::send_identity);
          break;

      case meta::hash_string<boost::mpl::string<'poli',
                                                 'cy_r',
                                                 'esol',
                                                 've'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::policy_resolve);
          break;

      case meta::hash_string<boost::mpl::string<'poli',
                                                 'cy_u',
                                                 'nres',
                                                 'olve'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::policy_unresolve);
          break;

      case meta::hash_string<boost::mpl::string<'poli',
                                                 'cy_u',
                                                 'pdat',
                                                 'e'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::policy_update);
          break;

      case meta::hash_string<boost::mpl::string<'endp',
                                                 'oint',
                                                 '_dec',
                                                 'lare'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::endpoint_declare);
          break;

      case meta::hash_string<boost::mpl::string<'endp',
                                                 'oint',
                                                 '_und',
                                                 'ecla',
                                                 're'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::endpoint_undeclare);
          break;

      case meta::hash_string<boost::mpl::string<'endp',
                                                 'oint',
                                                 '_res',
                                                 'olve'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::endpoint_resolve);
          break;

      case meta::hash_string<boost::mpl::string<'endp',
                                                 'oint',
                                                 '_unr',
                                                 'esol',
                                                 've'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::endpoint_unresolve);
          break;

      case meta::hash_string<boost::mpl::string<'endp',
                                                 'oint',
                                                 '_upd',
                                                 'ate'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::endpoint_update);
          break;

      case meta::hash_string<boost::mpl::string<'stat',
                                                 'e_re',
                                                 'port'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::state_report);
          break;

      case meta::hash_string<boost::mpl::string<'cust',
                                                 'om'
                                                > >::value:
          return PERFECT_RET_VAL(yajr::rpc::method::custom);
          break;

      default:
          return PERFECT_RET_VAL(yajr::rpc::method::unknown);
    }

