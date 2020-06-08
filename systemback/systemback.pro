TEMPLATE = subdirs

SUBDIRS += libsystemback \
           sbscheduler \
           sbsysupgrade \
           sbsustart \
           systemback \
           systemback-cli

sbscheduler.depends = libsystemback
sbsysupgrade.depends = libsystemback
sbsustart.depends = libsystemback
systemback.depends = libsystemback
systemback-cli.depends = libsystemback

TRANSLATIONS = lang/systemback_hu.ts \
               lang/systemback_ar_EG.ts \
               lang/systemback_ca_ES.ts \
               lang/systemback_cs.ts \
               lang/systemback_da_DK.ts \
               lang/systemback_de.ts \
               lang/systemback_en_GB.ts \
               lang/systemback_es.ts \
               lang/systemback_fi.ts \
               lang/systemback_fr.ts \
               lang/systemback_gl_ES.ts \
               lang/systemback_id.ts \
               lang/systemback_pt_BR.ts \
               lang/systemback_ro.ts \
               lang/systemback_ru.ts \
               lang/systemback_tr.ts \
               lang/systemback_uk.ts \
               lang/systemback_zh_CN.ts
