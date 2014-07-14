

typedef struct {
	char name[MAX_PATH];
	int  num_albums;
	Album * albums;
}


typedef struct {
	union {
		Artist *artist;
		Album *album;
		Song *song;
	};
	int error;
} GSFS_Query_FS_Result;


GSFS_Query_FS_Result gsfs_query_fs(
	char *artist_name)
{
	GSFS_Query_FS_Result result;
	
	result.error = ARTIST_NOT_FOUND;
	
	for(int i=0; i<artists.length; i++)
	{
		if (strcmp(artist[i].name, artist_name) == 0)
		{
			result.artist = &(artist[i]);
			result.error = SUCCESS;
			break;
		}
	}
}

GSFS_Query_FS_Result gsfs_query_fs(
	char *artist_name,
	char *album_name)
{
	GSFS_Query_FS_Result result;
	result = gsfs_query_fs(artist_name);
	
	if(result.error != 0)
		return result;
	
	result.error = ALBUM_NOT_FOUND;
	
	for(int i=0; i<result.artist.length; i++)
	{
		if (strcmp(artist.album[i]->name, album_name) == 0)
		{
			result.album = &(album[i]);
			result.error = SUCCESS;
			break;
		}
	}
}		

GSFS_Query_FS_Result gsfs_query_fs(
	char *artist_name,
	char *album_name,
	char *song_name)
{
	GSFS_Query_FS_Result result;
	
	result = gsfs_query_fs(artist_name);
	if(result.error != 0)
		return result;
		
	result = gsfs_query_fs(result.artist, album_name);
	if(result.error != 0)
		return result;
	
	result.error = SONG_NOT_FOUND;
	
	for(int i=0; i<result.album.length; i++)
	{
		if (strcmp(album.song[i]->name, song_name) == 0)
		{
			result.song = &(song[i]);
			result.error = SUCCESS;
			break;
		}
	}
	
	return result;
}

typedef enum {
	ROOT,
	ARTIST,
	ALBUM,
	SONG
} GSFS_Path_Level;

// A raw filesystem path looks like this:
// "/[artist]/[album]/[song.mp3]"
// We want to split it up into those three components
// so we declare three character arrays
// We also keep track of the 'level' a path has been
// parsed down to. So if the raw path is just "/", the
// level is "ROOT". If it's "/Daft Punk/", the level
// is ARTIST
typedef struct {
	GSFS_Path_Level level;
	char artist_name[MAX_PATH];
	char album_name[MAX_PATH];
	char song_name[MAX_PATH];
} GSFS_Path_Components;

// a function to break the raw path up into pieces
GSFS_Path_Components gsfs_parse_path(char* path) {
	// declare a component struct
	GSFS_Path_Components components;
	// i refers to the index we're using for the raw path
	// we can just ignore the first '/' so we start at i=1
	unsigned int i = 1;
	// j refers to the index we're copying characters to
	// in the component path
	unsigned int j = 0;
	
	components.level = ROOT;
	
	char *target = &artist_name;
	
	//loop until we get to the end of the string
	// which is signaled by a 0 character
	while(path[i] != 0) {
		//When we hit a '/', we know we've gotten to a new path component
		if(path[i] == '/')
		{
			//Increment the level
			components.level++;
			//All characters will go into the next
			//path component, which is MAX_PATH bits away
			target+=MAX_PATH;
			//reset j to zero now that we're in a new component
			j = 0;
		}
		//Otherwise, copy from the path into the target name
		else
		{
			//copy from the original path to the target path
			target[j] = path[i];
			j++
		}
		//look at the next character in the raw path
		i++
	}
	
	//if we get to the song-name level, strip out the extension
	if(components.level == SONG) {
		components.song_name[j-4] = '\0'; // '.'
		components.song_name[j-3] = '\0'; // 'm'
		components.song_name[j-2] = '\0'; // 'p'
		components.song_name[j-1] = '\0'; // '3'
	}	
}