<?php
$uploads_dir = 'ameba_recordings';

if ($_FILES['file']['error'] == UPLOAD_ERR_OK)
{
        $tmp_name = $_FILES['file']['tmp_name'];
        // $name = $_FILES['file']['name'];
        $date_str=date('YmdHis');
        move_uploaded_file($tmp_name, "$uploads_dir/$date_str".'.wav');
        
         try {
		 // 调用 api 部分，省略
                $result_str=$resp->getResult();
                $result_file=fopen("$uploads_dir/$date_str".'.txt',"a");
                fwrite($result_file,$result_str);
                fclose($result_file);
                echo $result_str;
        }
        catch(TencentCloudSDKException $e) {
                echo $e;
        }