<?php

// Set for readonly mode
$PETDISK_READ_ONLY = false;

function getParam($paramname)
{
    if (isset($_GET[$paramname]))
    {
        return $_GET[$paramname];
    }

    return false;
}

function fileExists($fileName, $caseSensitive = true) 
{
    if (file_exists($fileName)) 
    {
        return $fileName;
    }

    if ($caseSensitive)
    {
        return false;
    }

    // Handle case insensitive requests            
    $directoryName = dirname($fileName);
    $fileArray = glob($directoryName . '/*', GLOB_NOSORT);
    $fileNameLowerCase = strtolower($fileName);
    foreach ($fileArray as $file) 
    {
        if (strtolower($file) == $fileNameLowerCase) 
        {
            return $file;
        }
    }
    return false;
}

header_remove("X-Powered-By");

$verb = $_SERVER['REQUEST_METHOD'];
if ($verb == "GET")
{
    $file = "./";
    if (isset($_GET['file']))
    {
        if ($_GET['file'] == "TIME")
        {
            $file = "TIME";
        }
        else
        {
            $fname = "./".$_GET['file'];
            $file = fileExists($fname, false);
        }
    }

    if ($file)
    {
        if (isset($_GET['l']))
        {
            if ($_GET['l'] == 1)
            {
                $fs = 0;
                if ($file == "TIME")
                {
                    // length of time field
                    // this will be YYYY-MM-DD HH:mm:ss\n
                    $fs = strlen("YYYY-MM-DD HH:mm:ss\n");
                }
                else
                {
                    $fs = filesize($file);
                }
                $resp = $fs."\r\n";
                header('Content-Length: '.strlen($resp));
                header('Content-Type: application/octet-stream');
                echo $resp;
                flush();
            }
        }
        // directory request
        else if (getParam('d') == 1)
        {
            // directory
            // check for the index of the page requested.
            $page = -1;
            if (array_key_exists('p', $_GET))
            {
                $page = $_GET['p'];
            }

            $dir = ".";
            $files = scandir($dir);
            $filelist = "";
            $pagesize = 0;
            $max_page_size = 512;
            $dir_pages = array();
            $curr_page = 0;
            foreach ($files as $file)
            {
                $file_parts = pathinfo($file);
                $ext = "";
                if (array_key_exists('extension', $file_parts)) {
                    $ext = strtolower($file_parts['extension']);
                }

                if ($ext == "prg" || $ext == "seq" || $ext == "d64")
                {
                    $newentry = strtoupper($file)."\n";
                    if ($page >= 0 && $pagesize + strlen($newentry) >= $max_page_size)
                    {
                        // end this page and make a new page
                        $dir_pages[$curr_page] = $filelist."\n";
                        $curr_page++;
                        $filelist = "";
                    }
                    $filelist = $filelist.strtoupper($file)."\n";
                }

                $pagesize = strlen($filelist);
            }

            // last page
            $dir_pages[$curr_page] = $filelist."\n";
            
            $respbody = "\n";
            if ($page == -1)
            {
                $respbody = $dir_pages[0];
            }
            else
            {
                if ($page <= $curr_page)
                {
                    $respbody = $dir_pages[$page];
                }
            }

            header('Content-Length: '.strlen($respbody));
            header('Content-Type: application/octet-stream');
            echo $respbody;
            flush();
        }
        else
        {
            // requesting a range of bytes
            $start = getParam('s');
            $end = getParam('e');

            if ($file == "TIME")
            {
                $currentDate = new DateTime();
                $currentDate->setTimezone(new DateTimeZone("UTC"));
                $formattedDate = $currentDate->format("Y-m-d H:i:s\n");
                $content_length = strlen($formattedDate);
                header('Content-Length: '.$content_length);
                header('Content-Type: application/octet-stream');
                echo $formattedDate;
                flush();
                return;
            }

            if (fileExists($file)) {
                $contents = "";
                if ($end > 0) {
                    $fp = fopen($file, "r");
                    fseek($fp, $start, SEEK_SET);
                    $contents = fread($fp, $end-$start);
                    fclose($fp);
                    $content_length = $end-$start;
                } else {
                    $contents = file_get_contents($file);
                    $start = 0;
                    $end = strlen($contents);
                    $content_length = $end;
                }
                header('Content-Length: '.$content_length);
                header('Content-Type: application/octet-stream');
                echo $contents;
                flush();
            } else {
                error_log("file " . $file . " does not exist");
            }
        }
    }
}
else if ($verb == "PUT")
{
    if ($PETDISK_READ_ONLY == true)
    {
        // ignore writes for read only mode
        return;
    }
    $fname = $_GET['f'];
    $new = getParam('n');

    // read put data
    $putdata = '';
    if (getParam('b64')) {
        $base64data = file_get_contents("php://input");
        $putdata = base64_decode($base64data);
    } else {
        $putdata = file_get_contents("php://input");
    }

    $full_fname = "";
    if ($fname)
    {
        $full_fname = "./".$fname;
    }
        
    $exists = false;
    if ($full_fname != "")
    {
        $exists = file_exists($full_fname);
    }

    // remove existing file if new specified
    $file_contents = "";
    if ($exists == true && $new == 1)
    {
        unlink($full_fname);
        $exists = false;
    }

    if (getParam('u') == 1) // update specific block
    {
        $start = $_GET['s'];
        $end = $_GET['e'];

        // check to see if file is large enough
        $foundFname = fileExists($fname);
        if ($foundFname != false)
        {
            $fp = fopen($foundFname, "r+");
            fseek($fp, $start);
            fwrite($fp, $putdata, $end-$start);
            fclose($fp);
        }
        else
        {
            error_log("file not found: " . $fname);
        }
    }
    else // append block to end of file
    {
        if ($exists == true) 
        {
            $file_contents = file_get_contents($full_fname);
        }

        // append new data
        $file_contents = $file_contents . $putdata;
        // rewrite file
        file_put_contents($full_fname, $file_contents);
    }
}


?>
