/*
 * Copyright (C) 2016 - Marzo Sette Torres Junior
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This reader removes a lot of warnings of C-style casts. It should be included
// after GTK+ and GDK headers.
// No, I am not proud of this, but it works.

#ifndef INCL_GTK_REDEFINES
#define INCL_GTK_REDEFINES  1

namespace {
	template <typename T1, typename T2> 
	struct gtk_cast_internal { 
		static T1 *cast(T2 *obj) {
			return reinterpret_cast<T1 *>(obj);
		}
	}; 

	template <typename T> 
	struct gtk_cast_internal<T, T> { 
		static T *cast(T *obj) {
			return obj;
		}
	};
 }

template <typename T1, typename T2>
inline T1 *gtk_cast(T2 *obj) {
	return gtk_cast_internal<T1, T2>::cast(obj);
}

#if defined(_G_TYPE_CIC) || defined(_G_TYPE_CCC)
#  undef _G_TYPE_CIC
#  undef _G_TYPE_CCC
#  ifndef G_DISABLE_CAST_CHECKS
#    define _G_TYPE_CIC(ip, gt, ct)     gtk_cast<ct>(g_type_check_instance_cast(gtk_cast<GTypeInstance>((ip)), gt))
#    define _G_TYPE_CCC(cp, gt, ct)     gtk_cast<ct>(g_type_check_class_cast(gtk_cast<GTypeClass>((cp)), gt))
#  else /* G_DISABLE_CAST_CHECKS */
#    define _G_TYPE_CIC(ip, gt, ct)     gtk_cast<ct>((ip))
#    define _G_TYPE_CCC(cp, gt, ct)     gtk_cast<ct>((cp))
#  endif /* G_DISABLE_CAST_CHECKS */
#endif /* defined(_G_TYPE_CIC) || defined(_G_TYPE_CCC) */

#ifdef _G_TYPE_IGI
#  undef _G_TYPE_IGI
#  define _G_TYPE_IGI(ip, gt, ct)       gtk_cast<ct>(g_type_interface_peek(gtk_cast<GTypeInstance>((ip))->g_class, gt))
#endif /* _G_TYPE_IGI */

#ifdef G_CALLBACK
#  undef G_CALLBACK
#  define G_CALLBACK(f)       (reinterpret_cast<GCallback>((f)))
#endif /* G_CALLBACK */

#ifdef G_TYPE_MAKE_FUNDAMENTAL
#  undef G_TYPE_MAKE_FUNDAMENTAL
#define	G_TYPE_MAKE_FUNDAMENTAL(x)      (static_cast<GType>((x) << G_TYPE_FUNDAMENTAL_SHIFT))
#endif /* G_TYPE_MAKE_FUNDAMENTAL */
 
#if defined(g_list_previous) || defined(g_list_next)
#  undef g_list_previous
#  undef g_list_next
#define g_list_previous(list)           ((list) ? (gtk_cast<GList>((list))->prev) : nullptr)
#define g_list_next(list)               ((list) ? (gtk_cast<GList>((list))->next) : nullptr)
#endif /* defined(g_list_previous) || defined(g_list_next) */

#if defined(gtk_menu_append) || defined(gtk_menu_prepend) || defined(gtk_menu_insert)
#  undef gtk_menu_append
#  undef gtk_menu_prepend
#  undef gtk_menu_insert
#  define gtk_menu_append(menu,child)     gtk_menu_shell_append (gtk_cast<GtkMenuShell>((menu)),(child))
#  define gtk_menu_prepend(menu,child)    gtk_menu_shell_prepend(gtk_cast<GtkMenuShell>((menu)),(child))
#  define gtk_menu_insert(menu,child,pos) gtk_menu_shell_insert (gtk_cast<GtkMenuShell>((menu)),(child),(pos))
#endif /* defined(gtk_menu_append) || defined(gtk_menu_prepend) || defined(gtk_menu_insert) */

#ifdef g_signal_connect
#  undef g_signal_connect
#define g_signal_connect(instance, detailed_signal, c_handler, data) \
    g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), nullptr, static_cast<GConnectFlags>(0))
#endif /* g_signal_connect */

#ifdef g_utf8_next_char
#  undef g_utf8_next_char
#define g_utf8_next_char(p)   ((p) + g_utf8_skip[*reinterpret_cast<const guchar *>((p))])
#endif /* g_utf8_next_char */

#ifdef G_LOG_DOMAIN
#  undef G_LOG_DOMAIN
#  define G_LOG_DOMAIN    nullptr
#endif

#ifdef	gtk_signal_connect
#  undef gtk_signal_connect
#  define gtk_signal_connect(object,name,func,func_data)                                 \
    gtk_signal_connect_full((object), (name), (func), nullptr, (func_data), nullptr, 0, 0)
#endif

#ifdef gtk_signal_connect_object
#undef gtk_signal_connect_object
#  define gtk_signal_connect_object(object,name,func,slot_object)                        \
    gtk_signal_connect_full ((object), (name), (func), nullptr, (slot_object), nullptr, 1, 0)
#endif

#endif
