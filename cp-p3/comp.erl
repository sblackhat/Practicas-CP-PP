-module(comp).

-export([comp/1, comp/2 , decomp/1, decomp/2, comp_proc/2, decomp_proc/2, comp_proc/3, decomp_proc/3]).

-export([comp_loop_proc/3, decomp_loop_proc/3]).


-define(DEFAULT_CHUNK_SIZE, 1024*1024).

%%% File Compression

comp(File) -> %% Compress file to file.ch
    comp(File, ?DEFAULT_CHUNK_SIZE).

comp(File, Chunk_Size) ->  %% Starts a reader and a writer which run in separate processes
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    comp_loop(Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

comp_proc(File, Num_Threads) -> %% Compress file to file.ch
    comp_proc(File, ?DEFAULT_CHUNK_SIZE, Num_Threads).

comp_proc(File, Chunk_Size, Num_Threads) ->  %% Starts a reader and a writer which run in separate processes
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    init_procs(Num_Threads, Reader, Writer),
                    check_processes(Num_Threads),
                    Reader ! stop,
                    Writer ! stop;
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

comp_loop(Reader, Writer) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_loop(Reader, Writer);
        eof ->  %% end of file, stop reader and writer
            Reader ! stop,
            Writer ! stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n",[Reason]),
            Reader ! stop,
            Writer ! abort
    end.

%Creation of the compression process

init_procs(0, _, _) -> true;

init_procs(Num_Threads, Reader, Writer) -> spawn(comp, comp_loop_proc, [Reader, Writer, self()]),
                init_procs(Num_Threads-1, Reader, Writer).

%Check if the workers have finished

check_processes(0) -> ok;
                
check_processes(Num_Threads) -> 
    receive
        eof -> check_processes(Num_Threads-1)
    end.

comp_loop_proc(Reader, Writer, From) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_loop_proc(Reader, Writer, From);
        eof ->  %% end of file, stop reader and writer
            From ! eof;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n",[Reason]),
            Reader ! stop,
            Writer ! abort,
            From ! eof
    end.      


% File Decompression

decomp(Archive) ->
    decomp(Archive, string:replace(Archive, ".ch", "", trailing)).

decomp(Archive, Output_File) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    decomp_loop(Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

decomp_loop(Reader, Writer) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop(Reader, Writer);
        eof ->    %% end of file => exit decompression
            Reader ! stop,
            Writer ! stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n", [Reason]),
            Writer ! abort,
            Reader ! stop
    end.

%File decomp concurrent
decomp_proc(Archive, Num_Threads) ->  
    decomp_proc(Archive, string:replace(Archive, ".ch", "", trailing), Num_Threads).   

decomp_proc(Archive, Output_File, Num_Threads) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    init_procsD(Num_Threads, Reader, Writer),
                    check_processes(Num_Threads),
                    Reader ! stop,
                    Writer ! stop;
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

decomp_loop_proc(Reader, Writer, From) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive 
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop_proc(Reader, Writer, From);
        eof ->    %% end of file => exit decompression
            From ! eof;
        {error, Reason} -> 
            io:format("Error reading input file: ~w~n", [Reason]),
            Writer ! abort,
            Reader ! stop
    end.

%Create the decomp processes

init_procsD(0, _ , _) -> true;

init_procsD(Num_Threads, Reader, Writer) -> spawn(comp, decomp_loop_proc, [Reader, Writer, self()]),
                init_procsD(Num_Threads-1, Reader, Writer).