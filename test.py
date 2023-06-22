#!/usr/bin/python

import os, sys, subprocess
fs_size_bytes = 1048576

def spawn_lnxsh():
    global p
    p = subprocess.Popen('./lnxsh', shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

def issue(command):
    p.stdin.write((command + '\n').encode())

def check_fs_size():
    fs_size = os.path.getsize('disk')
    if fs_size > fs_size_bytes:
        print ("** File System is bigger than it should be (%s) **" %(pretty_size(fs_size)))

def do_exit():
    issue('exit')
    return p.communicate()[0]
#create a file that is larger than 8 blocks so that the file system is forced to use
#indirect block pointers
def test_large_file():
    issue('mkfs')
    # create a file large enough to use all the blocks in the system
    issue('create large_file 10000000')
    issue('stat large_file') # blocks: 1914, size: 975872
    issue('create f 100') #should fail
    check_fs_size();
    do_exit();

# create a directory with enough files that the directory needs to be resized into
# an inode with indirect pointers
def test_large_dir():
    issue('mkfs')
    issue('mkdir dir')
    issue('cd dir')
    for x in range(1, 121):
        issue('create f%d 1' %x)

    issue('ls')
    # test catting files before and after the boundary between blocks 8 and 9
    issue('cat f118') #A
    issue('cat f119') #A
    issue('cat f120') #A
    issue('cd ..')
    issue('stat dir')
    do_exit()

def test_dir_frag():
    issue('mkfs')
    issue('mkdir dir')
    issue('cd dir')
    for x in range(1, 121):
        issue('create f%d 1' %x)
    issue('stat .') # should take up 10 blocks
    for x in range(119, 121):
        issue('unlink f%d' %x)
    issue('stat .') # should take up 8 blocks, removes indirect block
    for x in range(119, 125):
        issue('create f%d 1' %x)
    issue('ls') #all files from f1 to f124 should be present
    do_exit()

# test that interleaved reads and writes on same file but different
# descriptors work
def test_multi_open():
    issue('mkfs')
    issue('open f 2')
    issue('open f 1')
    issue('open f 3')
    issue('write 0 hello')
    issue('read 1 5') # hello
    issue('write 2 thereworld')
    issue('read 1 5') # world
    issue('lseek 1 0')
    issue('read 1 5') # there
    do_exit()

# make sure we fail when there are no file handles left
def test_get_all_handles():
    issue('mkfs')
    for x in range(0,257):
        issue('open f%d 3' %x) #should fail after file handle 255
    do_exit()

# make sure we fail when all inodes are used
def test_get_all_inodes():
    issue('mkfs')
    for x in range(0,2050):
        issue('open f%d 3' %x) #should fail after 2047, 3 times
        issue('close 0')
    do_exit()


print ("......Starting my tests\n\n")
sys.stdout.flush()
# spawn_lnxsh()
# test_large_file()
# spawn_lnxsh()
# test_large_dir()
# spawn_lnxsh()
# test_dir_frag()
# spawn_lnxsh()
# test_multi_open()
# spawn_lnxsh()
# test_get_all_handles()
# spawn_lnxsh()
# test_get_all_inodes()
# spawn_lnxsh()

# Testa a função mkfs
def check_mkfs():
    spawn_lnxsh()
    issue("mkfs")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # Goodbye\n")

    disk_size = os.path.getsize('disk')

    if(output.decode() == expected and disk_size == 30720):
        print("Sistema de arquivos montado com sucesso")

# Testa a criação de um arquivo com o comando open
def check_open():
    spawn_lnxsh()
    issue("mkfs")
    issue("open teste.txt 3")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # File handle is : 0\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Arquivo criado com sucesso")

# Testa o fechamento de um descritor de arquivos com o comando close
def check_close():
    spawn_lnxsh()
    issue("mkfs")
    issue("open arquivo.txt 2")
    issue("close 0")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # File handle is : 0\n"
                "# OK\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Descritor de arquivos fechado com sucesso")

# Testa execução do comando ls
def check_ls():
    spawn_lnxsh()
    issue("mkfs")
    issue("open arquivo1.txt 2")
    issue("open arquivo2.txt 3")
    issue("open arquivo3.txt 2")
    issue("open arquivo4.txt 2")
    issue("ls")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # File handle is : 0\n"
                "# File handle is : 1\n"
                "# File handle is : 2\n"
                "# File handle is : 3\n"
                "# .\n"
                "..\n"
                "arquivo1.txt\n"
                "arquivo2.txt\n"
                "arquivo3.txt\n"
                "arquivo4.txt\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Comando ls executado com sucesso")

# Testa criação de um diretório chamado 'sistemas'
def check_mkdir():
    spawn_lnxsh()
    issue("mkfs")
    issue("mkdir sistemas")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # OK\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Diretório criado com sucesso")

# Testa remoção de um diretório chamado 'sistemas'
def check_rmdir():
    spawn_lnxsh()
    issue("mkfs")
    issue("mkdir sistemas")
    issue("rmdir sistemas")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # OK\n"
                "# OK\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Diretório removido com sucesso")

# Testa criação de um link chamado file.txt para um arquivo chamado arquivo.txt
def check_link():
    spawn_lnxsh()
    issue("mkfs")
    issue("open arquivo.txt 3")
    issue("link arquivo.txt file.txt")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# File handle is : 0\n"
                "# # Goodbye\n")

    if(output.decode() == expected):
        print("Link criado com sucesso")

# Testa remoção de um link chamado file.txt para um arquivo chamado arquivo.txt
def check_unlink():
    spawn_lnxsh()
    issue("mkfs")
    issue("open arquivo.txt 3")
    issue("link arquivo.txt file.txt")
    issue("unlink file.txt")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # File handle is : 0\n"
                "# # # Goodbye\n")

    if(output.decode() == expected):
        print("Link removido com sucesso")

# Testa remoção de um arquivo chamado arquivo.txt usando o comando unlink
def check_file_delete():
    spawn_lnxsh()
    issue("mkfs")
    issue("open arquivo.txt 3")
    issue("close 0") # fecha o descritor ja que para remover um arquivo não pode ter descritor aberto
    issue("unlink arquivo.txt")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # File handle is : 0\n"
                "# OK\n"
                "# # Goodbye\n")

    if(output.decode() == expected):
        print("Arquivo removido com sucesso")

# Testa criação de um arquivo chamado arquivo.txt com o tamanho máximo (70656 bytes - usando single indirect)
def check_file_create():
    spawn_lnxsh()
    issue("mkfs")
    issue("create arquivo.txt 70656")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # # Goodbye\n")

    if(output.decode() == expected):
        print("Arquivo de 70656 bytes criado com sucesso")

# Testa criação de um arquivo chamado arquivo.txt com o tamanho máximo (70656 bytes - usando single indirect) e exibe suas informações usando comando stat
def check_stat():
    spawn_lnxsh()
    issue("mkfs")
    issue("create arquivo.txt 70656")
    issue("stat arquivo.txt")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # #     Inode No         : 1\n"
                "    Type             : FILE\n"
                "    Link Count       : 1\n"
                "    Size             : 71168\n" # 70656 bytes do arquivo + 512 bytes para alocação de um bloco para ponteiros single indirect
                "    Blocks allocated : 139\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Comando stat executado com sucesso")

# Testa uso dos comandos lseek e write criando um arquivo chamado arquivo.txt, movendo o ponteiro para posição 5121 do arquivo e escrevendo 'sistemasoperacionais' a partir dessa posição
def check_lseek_and_write():
    spawn_lnxsh()
    issue("mkfs")
    issue("open arquivo.txt 3")
    issue("lseek 0 5121")
    issue("write 0 sistemasoperacionais")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # File handle is : 0\n"
                "# OK\n"
                "# Done\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Comandos lseek e write executados com sucesso")

# Testa uso do comando read através do comando cat (utiliza o comando read implementado internamente)
def check_read():
    spawn_lnxsh()
    issue("mkfs")
    issue("create arquivo.txt 10") # cria arquivo com 10 bytes (escreve de A - J no arquivo)
    issue("cat arquivo.txt") # Imprime de A - J conforme esperado

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # # ABCDEFGHIJ\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Comando read executado com sucesso")

# Testa o uso do comando cd implementado
def check_cd():
    spawn_lnxsh()
    issue("mkfs")
    issue("mkdir sistemas")
    issue("cd sistemas")

    output = do_exit()
    expected = ("ShellShock Version 0.000003\n"
                "\n"
                "# # OK\n"
                "# OK\n"
                "# Goodbye\n")

    if(output.decode() == expected):
        print("Comando cd executado com sucesso")

# Nossos testes (pode levar alguns segundos para rodar todos os testes)
check_mkfs()
check_open()
check_close()
check_ls()
check_mkdir()
check_rmdir()
check_link()
check_unlink()
check_file_delete()
check_lseek_and_write()
check_read()
check_cd()
check_stat()
check_file_create()