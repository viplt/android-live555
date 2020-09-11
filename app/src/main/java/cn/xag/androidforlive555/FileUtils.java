package cn.xag.androidforlive555;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;

public class FileUtils {
    public static void deleteFile(String filePath){
        if(filePath == null || filePath.trim().length() == 0){
            return ;
        }
        File file = new File(filePath);
        if(file==null || !file.exists()){
            return ;
        }
        file.delete();
    }

    /**
     * 获取文件名称
     * @return
     */
    public static String getFileNameByTime(){
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyyMMddkkmmss");
        return simpleDateFormat.format(new Date());
    }
}
