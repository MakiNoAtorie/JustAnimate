# User manual: `cmake --build <build> --target manual` writes the HTML pages, the joined
# Markdown and the PDF into <build>/manual. Pandoc is required; Typst is required for the PDF.
find_program(PANDOC_EXECUTABLE pandoc)
find_program(TYPST_EXECUTABLE typst)

set(MANUAL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)
set(MANUAL_OUTPUT_DIR ${CMAKE_BINARY_DIR}/manual)
file(MAKE_DIRECTORY ${MANUAL_OUTPUT_DIR}/images)
file(GLOB MANUAL_CHAPTERS CONFIGURE_DEPENDS ${MANUAL_SOURCE_DIR}/[0-9][0-9]-*.md)

if(NOT PANDOC_EXECUTABLE)
    add_custom_target(manual
        COMMAND ${CMAKE_COMMAND} -E echo "manual: Pandoc not found; install it and re-run cmake"
        COMMAND ${CMAKE_COMMAND} -E false)
    return()
endif()

set(MANUAL_PANDOC ${PANDOC_EXECUTABLE} --from markdown --lua-filter ${MANUAL_SOURCE_DIR}/links.lua)
set(MANUAL_JOINED ${MANUAL_OUTPUT_DIR}/JustAnimate-manual.md)

# The joined Markdown is built first: it checks every chapter before any page is written.
set(MANUAL_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MANUAL_SOURCE_DIR}/manual.css ${MANUAL_OUTPUT_DIR}
    COMMAND ${MANUAL_PANDOC} -M manual-format=joined --to markdown --standalone
        -M "title=JustAnimate User Manual" -o ${MANUAL_JOINED} ${MANUAL_CHAPTERS})
foreach(chapter ${MANUAL_CHAPTERS})
    get_filename_component(page ${chapter} NAME_WE)
    if(page STREQUAL "00-index")
        set(page index)
    endif()
    list(APPEND MANUAL_COMMANDS
        COMMAND ${MANUAL_PANDOC} -M manual-format=html --to html5 --standalone
            --template ${MANUAL_SOURCE_DIR}/template.html -o ${page}.html ${chapter})
endforeach()

if(TYPST_EXECUTABLE)
    list(APPEND MANUAL_COMMANDS
        # Typst gets a .typ file, not Pandoc's PDF path: that path gives Typst absolute image paths, which it refuses.
        COMMAND ${PANDOC_EXECUTABLE} ${MANUAL_JOINED} --to typst --standalone
            --toc --toc-depth=2 -V papersize=a4 -V page-numbering=1
            "--variable=header-includes=#show heading.where(level: 1): it => pagebreak(weak: true) + it"
            -o JustAnimate-manual.typ
        COMMAND ${TYPST_EXECUTABLE} compile JustAnimate-manual.typ JustAnimate-manual.pdf)
else()
    list(APPEND MANUAL_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E echo "manual: Typst not found; the PDF is not built")
endif()

add_custom_target(manual
    ${MANUAL_COMMANDS}
    WORKING_DIRECTORY ${MANUAL_OUTPUT_DIR}
    SOURCES ${MANUAL_CHAPTERS} ${MANUAL_SOURCE_DIR}/links.lua
        ${MANUAL_SOURCE_DIR}/template.html ${MANUAL_SOURCE_DIR}/manual.css
    VERBATIM
)
