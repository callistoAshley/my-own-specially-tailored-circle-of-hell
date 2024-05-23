ruby = find_program('ruby', native: true)

global_sources += custom_target('pack scripts',
    output: 'xScripts.rxdata',
    command: [
            ruby,
            meson.current_source_dir() + '/utils/rpgscript.rb',
            '-s', meson.current_source_dir() + '/oneshot',
            '-o', '@OUTPUT@'
        ],
    install: true,
    install_dir: 'Data',
    build_always_stale: true
)