#include <dirent.h>
#include <errno.h>
#include "log.h"
#include <sys/wait.h>
#include <unistd.h>

Logger logger(2048);

typedef enum
{
    eFileType_None,
    eFileType_Folder,
    eFileType_Media,
    eFileType_Cue,
    eFileType_Unknown,
    eFileType_UnSupported,
}eFileType;

eFileType get_file_type(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
    {
        logger(LEVEL_ERROR, "Stat Failed: %s", path.c_str());
        return eFileType_None;
    }
    switch (sb.st_mode & S_IFMT)
    {
        case S_IFDIR:
            return eFileType_Folder;
        case S_IFREG:
        {
            int index = path.find_last_of('.');
            std::string file_type = path.substr(index + 1, path.size() - index);
            if (file_type == "m4a")
            {
                return eFileType_Media;
            }
            else if (file_type == "cue")
            {
                return eFileType_Cue;
            }
            else
            {
                return eFileType_UnSupported;
            }
        }
        default:
            return eFileType_Unknown;
    }
}

int exec_cmd(const char *name, std::vector<std::string> &cmd_args_)
{
	pid_t pid;
    int ret = 0;

    const char *cmd_args[cmd_args_.size() + 1];
    //cmd_args = new const char*[cmd_args_.size() + 1];
    for (int i = 0; i < cmd_args_.size(); i++)
    {
        cmd_args[i] = cmd_args_[i].c_str();
    }
    cmd_args[cmd_args_.size()] = NULL;

	pid = fork();
    if (pid == -1)
        return -ENOMEM;

    else if (pid != 0 )
    {//parent
		pid_t child_pid;
		int stat_val;
		/*
		 * WIFEXITED(stat_val): Non-zero if the child is terminated
		 * normally.
		 *
		 * WEXITSTATUS(stat_val): if WIFEXITED is non-zero, this
		 * returns child exit code.
		 *
		 * WIFSIGNALED(stat_val): Non-zero if the child is terminated
		 * on an uncaught signal.
		 *
		 * WTERMSIG(stat_val): if WIFSIGNALED is non-zero, this
		 * returns a signal number.
		 *
		 * WIFSTOPPED(stat_val): Non-zero if the child has stopped.
		 *
		 * WSTOPSIG(stat_val): if WIFSTOPPED is non-zero, this returns
		 * a signal number.
		 */
        child_pid = waitpid(pid, &stat_val, 0);
        if (child_pid == -1) {
            /*
             * TBD: check ther errno.
             *
             * Could be:
             * ECHILD: no child process;
             * EINTR: interrupted by signal;
             * EINVAL: invalid option argument
             */
        }
		if (WIFEXITED(stat_val)) {
			//printf("Child exited with code:%d\n", WEXITSTATUS(stat_val));
        } else if(WIFSIGNALED(stat_val)) {
			printf("Child interrupted by signal: %d\n", WTERMSIG(stat_val));
            ret = -EINVAL;
        } else {
			printf("Child stopped unknown\n");
            ret = -EINVAL;
        }
	} else {
		/* This is the child */
        return execvp(name, (char **)cmd_args);
	}
	return ret;
}

int process_dir(const std::string &src_path,
        const std::string &artist,
        const std::string &album)
{
    DIR *dir;
    struct dirent *ptr;
    int ret = 0;

    dir = opendir(src_path.c_str());
    if (dir == nullptr)
    {
        logger(LEVEL_ERROR, "Dir open err: %s", src_path.c_str());
        return -EINVAL;
    }
    while ((ptr = readdir(dir)) != nullptr)
    {
        std::string filename(ptr->d_name);
        if (filename == "." || filename == "..")
            continue;

        //std::cout << "--name: " << ptr->d_name << std::endl;
        eFileType type = get_file_type(src_path + "/" + ptr->d_name);
        switch (type)
        {
            case eFileType_Media:
            {
                std::vector<std::string> cmd_args_;
                std::string cmd_log = "AtomicParsley ";
                cmd_args_.push_back("AtomicParsley");
                //set target file
                cmd_args_.push_back(src_path + "/" + filename);
                cmd_log += src_path + "/" + filename;
                cmd_log += " ";
                //set artist
                if (artist.size() != 0)
                {
                    cmd_args_.push_back("--albumArtist");
                    cmd_log += "--albumArtist ";
                    cmd_args_.push_back(artist);
                    cmd_log += artist;
                    cmd_log += " ";
                }
                //set album
                if (album.size() != 0)
                {
                    cmd_args_.push_back("--album");
                    cmd_log += "--album ";
                    cmd_args_.push_back(album);
                    cmd_log += album;
                    cmd_log += " ";
                }
                cmd_args_.push_back("--overWrite");
                cmd_log += "--overWrite";
                logger(LEVEL_INFO, "cmd: %s", cmd_log.c_str());
                exec_cmd("AtomicParsley", cmd_args_);
                break;
            }
            case eFileType_Folder:
                process_dir(src_path + "/" + ptr->d_name, artist, album);
                break;
            default:
                break;
        }
    }
    closedir(dir);
    return 0;
}

int main(int argc, char *argv[])
{
    int c;
    std::string artist, album, discid, folder;;

    while( (c = getopt(argc, argv, "a:t:i:d:")) != -1)
    {
        switch (c)
        {
            case 'a':
                artist = optarg;
                break;
            case 't':
                album = optarg;
                break;
            case 'i':
                discid = optarg;
                break;
            case 'd':
                folder = optarg;
                break;
            case '?':
                return -EINVAL;
            default:
                return -1;
        }
    }
    if (artist.size() == 0 && album.size() == 0 && discid.size() == 0)
    {
        logger(LEVEL_ERROR, "Should at least set artist or album or discid");
        goto error;
    }
    if (folder.size() == 0)
    {
        logger(LEVEL_ERROR, "Should set target folder");
        goto error;
    }

    if (discid.size() != 0)
    {
        logger(LEVEL_INFO, "Input DISC ID: %s", discid.c_str());
    }
    if (artist.size() != 0)
    {
        logger(LEVEL_INFO, "Override artist: %s", artist.c_str());
    }
    if (album.size() != 0)
    {
        logger(LEVEL_INFO, "Override album: %s", album.c_str());
    }

    process_dir(folder, artist, album);
    return 0;
error:
    logger(LEVEL_INFO, "usage: %s -d <folder> -a <artist> -t <album> -i <discid>", argv[0]);
    return -EINVAL;
}
