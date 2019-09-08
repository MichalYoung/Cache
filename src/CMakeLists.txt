#
# One piece at a time ...
#


include_directories(
        /usr/local/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)


add_custom_command(
        OUTPUT gram.c gram.h
        COMMAND bison -d -o ${CMAKE_CURRENT_BINARY_DIR}/gram.c  ${CMAKE_CURRENT_SOURCE_DIR}/gram.y
        DEPENDS gram.y timestamp.h
)


add_custom_command(
        OUTPUT agram.c agram.h
        COMMAND bison -d -o ${CMAKE_CURRENT_BINARY_DIR}/agram.c -p a_ ${CMAKE_CURRENT_SOURCE_DIR}/agram.y
        DEPENDS agram.y gram.h
)

add_custom_command(
        OUTPUT scan.c scan.h
        COMMAND flex -o ${CMAKE_CURRENT_BINARY_DIR}/scan.c ${CMAKE_CURRENT_SOURCE_DIR}/scan.l
        DEPENDS  gram.h scan.l
)

add_executable(assembler
        gram.c gram.h
        assemble.c assembler.c assembler.h
        code.c stack.c timestamp.c timestamp.h
        hwdb.c table.c topic.c
        sqlstmts.c parser.c
        scan.c rtab.c typetable.c ptable.c
        nodecrawler.c mb.c indextable.c event.c dsemem.c
        automaton.c agram.c disassemble.c
        )

target_link_libraries(assembler
        /usr/local/lib/libADTs.a
        /usr/local/lib/libsrpc.a
        )
