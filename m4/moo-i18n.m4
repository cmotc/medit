##############################################################################
# MOO_AC_I18N
#
AC_DEFUN([MOO_AC_I18N],[
    AC_PROG_INTLTOOL([0.29])

    GETTEXT_PACKAGE=$1
    AC_SUBST(GETTEXT_PACKAGE)
    AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the gettext package.])

    ALL_LINGUAS="$2"
    AM_GLIB_GNU_GETTEXT

    MOO_INTLTOOL_INI_RULE='%.ini: %.ini.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
    AC_SUBST(MOO_INTLTOOL_INI_RULE)
])