//  Boost io/ios_state.hpp header file  --------------------------------------//
//  Slightly simplified for Exult.
//  Copyright 2002, 2005 Daryle Walker.  Use, modification, and distribution
//  are subject to the Boost Software License, Version 1.0.  (See accompanying
//  file LICENSE_1_0.txt or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

//  See <http://www.boost.org/libs/io/> for the library's home page.

#ifndef BOOST_IO_IOS_STATE_HPP
#define BOOST_IO_IOS_STATE_HPP

#ifndef BOOST_NO_STD_LOCALE
#	include <locale>    // for std::locale
#endif

#include <ios>          // for std::ios_base, std::basic_ios, etc.
#include <ostream>      // for std::basic_ostream
#include <streambuf>    // for std::basic_streambuf
#include <string>       // for std::char_traits

namespace boost { namespace io {

	//  Basic stream state saver class declarations
	//  -----------------------------//

	class ios_flags_saver {
	public:
		using state_type  = ::std::ios_base;
		using aspect_type = ::std::ios_base::fmtflags;

		explicit ios_flags_saver(state_type& s)
				: s_save_(s), a_save_(s.flags()) {}

		ios_flags_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.flags(a)) {}

		~ios_flags_saver() {
			this->restore();
		}

		ios_flags_saver& operator=(const ios_flags_saver&) = delete;

		void restore() {
			s_save_.flags(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	class ios_precision_saver {
	public:
		using state_type  = ::std::ios_base;
		using aspect_type = ::std::streamsize;

		explicit ios_precision_saver(state_type& s)
				: s_save_(s), a_save_(s.precision()) {}

		ios_precision_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.precision(a)) {}

		~ios_precision_saver() {
			this->restore();
		}

		ios_precision_saver& operator=(const ios_precision_saver&) = delete;

		void restore() {
			s_save_.precision(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	class ios_width_saver {
	public:
		using state_type  = ::std::ios_base;
		using aspect_type = ::std::streamsize;

		explicit ios_width_saver(state_type& s)
				: s_save_(s), a_save_(s.width()) {}

		ios_width_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.width(a)) {}

		~ios_width_saver() {
			this->restore();
		}

		ios_width_saver& operator=(const ios_width_saver&) = delete;

		void restore() {
			s_save_.width(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	//  Advanced stream state saver class template declarations
	//  -----------------//

	template <typename Ch, class Tr>
	class basic_ios_iostate_saver {
	public:
		using state_type  = ::std::basic_ios<Ch, Tr>;
		using aspect_type = ::std::ios_base::iostate;

		explicit basic_ios_iostate_saver(state_type& s)
				: s_save_(s), a_save_(s.rdstate()) {}

		basic_ios_iostate_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.rdstate()) {
			s.clear(a);
		}

		~basic_ios_iostate_saver() {
			this->restore();
		}

		basic_ios_iostate_saver& operator=(const basic_ios_iostate_saver&)
				= delete;

		void restore() {
			s_save_.clear(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	template <typename Ch, class Tr>
	class basic_ios_exception_saver {
	public:
		using state_type  = ::std::basic_ios<Ch, Tr>;
		using aspect_type = ::std::ios_base::iostate;

		explicit basic_ios_exception_saver(state_type& s)
				: s_save_(s), a_save_(s.exceptions()) {}

		basic_ios_exception_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.exceptions()) {
			s.exceptions(a);
		}

		~basic_ios_exception_saver() {
			this->restore();
		}

		basic_ios_exception_saver& operator=(const basic_ios_exception_saver&)
				= delete;

		void restore() {
			s_save_.exceptions(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	template <typename Ch, class Tr>
	class basic_ios_tie_saver {
	public:
		using state_type  = ::std::basic_ios<Ch, Tr>;
		using aspect_type = ::std::basic_ostream<Ch, Tr>*;

		explicit basic_ios_tie_saver(state_type& s)
				: s_save_(s), a_save_(s.tie()) {}

		basic_ios_tie_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.tie(a)) {}

		~basic_ios_tie_saver() {
			this->restore();
		}

		basic_ios_tie_saver& operator=(const basic_ios_tie_saver&) = delete;

		void restore() {
			s_save_.tie(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	template <typename Ch, class Tr>
	class basic_ios_rdbuf_saver {
	public:
		using state_type  = ::std::basic_ios<Ch, Tr>;
		using aspect_type = ::std::basic_streambuf<Ch, Tr>*;

		explicit basic_ios_rdbuf_saver(state_type& s)
				: s_save_(s), a_save_(s.rdbuf()) {}

		basic_ios_rdbuf_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.rdbuf(a)) {}

		~basic_ios_rdbuf_saver() {
			this->restore();
		}

		basic_ios_rdbuf_saver& operator=(const basic_ios_rdbuf_saver&) = delete;

		void restore() {
			s_save_.rdbuf(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	template <typename Ch, class Tr>
	class basic_ios_fill_saver {
	public:
		using state_type  = ::std::basic_ios<Ch, Tr>;
		using aspect_type = typename state_type::char_type;

		explicit basic_ios_fill_saver(state_type& s)
				: s_save_(s), a_save_(s.fill()) {}

		basic_ios_fill_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.fill(a)) {}

		~basic_ios_fill_saver() {
			this->restore();
		}

		basic_ios_fill_saver& operator=(const basic_ios_fill_saver&) = delete;

		void restore() {
			s_save_.fill(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};

	using ios_fill_saver = basic_ios_fill_saver<char, std::char_traits<char>>;

#ifndef BOOST_NO_STD_LOCALE
	template <typename Ch, class Tr>
	class basic_ios_locale_saver {
	public:
		using state_type  = ::std::basic_ios<Ch, Tr>;
		using aspect_type = ::std::locale;

		explicit basic_ios_locale_saver(state_type& s)
				: s_save_(s), a_save_(s.getloc()) {}

		basic_ios_locale_saver(state_type& s, const aspect_type& a)
				: s_save_(s), a_save_(s.imbue(a)) {}

		~basic_ios_locale_saver() {
			this->restore();
		}

		basic_ios_locale_saver& operator=(const basic_ios_locale_saver&)
				= delete;

		void restore() {
			s_save_.imbue(a_save_);
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
	};
#endif

	//  User-defined stream state saver class declarations
	//  ----------------------//

	class ios_iword_saver {
	public:
		using state_type  = ::std::ios_base;
		using index_type  = int;
		using aspect_type = long;

		explicit ios_iword_saver(state_type& s, index_type i)
				: s_save_(s), a_save_(s.iword(i)), i_save_(i) {}

		ios_iword_saver(state_type& s, index_type i, const aspect_type& a)
				: s_save_(s), a_save_(s.iword(i)), i_save_(i) {
			s.iword(i) = a;
		}

		~ios_iword_saver() {
			this->restore();
		}

		ios_iword_saver& operator=(const ios_iword_saver&) = delete;

		void restore() {
			s_save_.iword(i_save_) = a_save_;
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
		const index_type  i_save_;
	};

	class ios_pword_saver {
	public:
		using state_type  = ::std::ios_base;
		using index_type  = int;
		using aspect_type = void*;

		explicit ios_pword_saver(state_type& s, index_type i)
				: s_save_(s), a_save_(s.pword(i)), i_save_(i) {}

		ios_pword_saver(state_type& s, index_type i, const aspect_type& a)
				: s_save_(s), a_save_(s.pword(i)), i_save_(i) {
			s.pword(i) = a;
		}

		~ios_pword_saver() {
			this->restore();
		}

		ios_pword_saver operator=(const ios_pword_saver&) = delete;

		void restore() {
			s_save_.pword(i_save_) = a_save_;
		}

	private:
		state_type&       s_save_;
		const aspect_type a_save_;
		const index_type  i_save_;
	};

	//  Combined stream state saver class (template) declarations
	//  ---------------//

	class ios_base_all_saver {
	public:
		using state_type = ::std::ios_base;

		explicit ios_base_all_saver(state_type& s)
				: s_save_(s), a1_save_(s.flags()), a2_save_(s.precision()),
				  a3_save_(s.width()) {}

		~ios_base_all_saver() {
			this->restore();
		}

		ios_base_all_saver& operator=(const ios_base_all_saver&) = delete;

		void restore() {
			s_save_.width(a3_save_);
			s_save_.precision(a2_save_);
			s_save_.flags(a1_save_);
		}

	private:
		state_type&                s_save_;
		const state_type::fmtflags a1_save_;
		const ::std::streamsize    a2_save_;
		const ::std::streamsize    a3_save_;
	};

	template <typename Ch, class Tr>
	class basic_ios_all_saver {
	public:
		using state_type = ::std::basic_ios<Ch, Tr>;

		explicit basic_ios_all_saver(state_type& s)
				: s_save_(s), a1_save_(s.flags()), a2_save_(s.precision()),
				  a3_save_(s.width()), a4_save_(s.rdstate()),
				  a5_save_(s.exceptions()), a6_save_(s.tie()),
				  a7_save_(s.rdbuf()), a8_save_(s.fill())
#ifndef BOOST_NO_STD_LOCALE
				  ,
				  a9_save_(s.getloc())
#endif
		{
		}

		~basic_ios_all_saver() {
			this->restore();
		}

		basic_ios_all_saver& operator=(const basic_ios_all_saver&) = delete;

		void restore() {
#ifndef BOOST_NO_STD_LOCALE
			s_save_.imbue(a9_save_);
#endif
			s_save_.fill(a8_save_);
			s_save_.rdbuf(a7_save_);
			s_save_.tie(a6_save_);
			s_save_.exceptions(a5_save_);
			s_save_.clear(a4_save_);
			s_save_.width(a3_save_);
			s_save_.precision(a2_save_);
			s_save_.flags(a1_save_);
		}

	private:
		state_type&                           s_save_;
		const typename state_type::fmtflags   a1_save_;
		const ::std::streamsize               a2_save_;
		const ::std::streamsize               a3_save_;
		const typename state_type::iostate    a4_save_;
		const typename state_type::iostate    a5_save_;
		::std::basic_ostream<Ch, Tr>* const   a6_save_;
		::std::basic_streambuf<Ch, Tr>* const a7_save_;
		const typename state_type::char_type  a8_save_;
#ifndef BOOST_NO_STD_LOCALE
		const ::std::locale a9_save_;
#endif
	};

	class ios_all_word_saver {
	public:
		using state_type = ::std::ios_base;
		using index_type = int;

		ios_all_word_saver(state_type& s, index_type i)
				: s_save_(s), i_save_(i), a1_save_(s.iword(i)),
				  a2_save_(s.pword(i)) {}

		~ios_all_word_saver() {
			this->restore();
		}

		ios_all_word_saver& operator=(const ios_all_word_saver&) = delete;

		void restore() {
			s_save_.pword(i_save_) = a2_save_;
			s_save_.iword(i_save_) = a1_save_;
		}

	private:
		state_type&      s_save_;
		const index_type i_save_;
		const long       a1_save_;
		void* const      a2_save_;
	};

}}    // namespace boost::io

#endif    // BOOST_IO_IOS_STATE_HPP
