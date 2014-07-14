typedef enum {
	ROOT,
	ARTIST,
	ALBUM
} Path_Level;


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