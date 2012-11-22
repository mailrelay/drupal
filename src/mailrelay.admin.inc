<?php
// defines the form for the sync process
function mailrelayForm($options, $apiKey)
{
	$form = array();

	// info text
	$form['info'] = array(
        	'#value' => t('This button will sync all users to Mailrelay.')
        );

	// placeholder for API key (we will pass it internally)
	$form["apiKey"] = array(
		'#type'=>'value',
		'#value'=>$apiKey
	);

	// select with all the mailrelay groups
	$form["groups"] = array(
        	'#title'=>t('Select the groups you want to sync'),
                '#type'=>'select',
                '#multiple'=>true,
                '#options'=>$options
        );

	// submit button that starts the sync process
	$form["submit"] = array(
		'#type'=>'submit',
		'#value'=>t('Start sync process')
	);

	return $form;
}

// main screen for sync process: gets config vars and connects to API to get groups
function mailrelayStartSync() 
{
	$username = variable_get('mailrelay_username', '');
	$password = variable_get('mailrelay_password', '');
	$hostname = variable_get('mailrelay_hostname', '');
    	
	$form = array();    
	if (!$username || !$password || !$hostname) 
	{
		// error in settings
        	drupal_set_message(t('Mailrelay Sync has not been properly configured. Syncing will not work until you finish configuration'), 'error');
		return false;
	}
	else
    	{
		// verify
		if (substr($hostname, 0, 7)=="http://") $hostname = substr($hostname, 7);

		// First thing, authenticate
    		$url = 'http://'. $hostname .'/ccm/admin/api/version/2/&type=json';
    		$curl = curl_init($url);

    		$params = array(
    			'function' => 'doAuthentication',
    			'username' => $username,
    			'password' => $password
    		);

		// call to authenticate
    		curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
    		curl_setopt($curl, CURLOPT_POST, 1);
    		curl_setopt($curl, CURLOPT_POSTFIELDS, $params);

                $headers = array(
                        'X-Request-Origin: Drupal7|1.1|'.VERSION
                );
                curl_setopt($curl, CURLOPT_HTTPHEADER, $headers);

    		// Call the page, it will return a json
    		$result = curl_exec($curl);
	    	$jsonResult = json_decode($result);

    		if (!$jsonResult->status)
    		{
        		// show error
        		drupal_set_message(t('User data are incorrect.'), 'error');
	        	return false;
    		}
    		else
    		{
        		//If the user validates correctly, we will get the groups data
        		$apiKey = $jsonResult->data;
	
        		// Call getGroups
        		$params = array(
            			'function' => 'getGroups',
            			'apiKey' => $apiKey
        		);

	                $headers = array(
        	                'X-Request-Origin: Drupal7|1.1|'.VERSION
        	        );
                	curl_setopt($curl, CURLOPT_HTTPHEADER, $headers);

       	 		curl_setopt($curl, CURLOPT_POSTFIELDS, $params);

        		$result = curl_exec($curl);
        		$jsonResult = json_decode($result);

        		if (!$jsonResult->status)
        		{
				// error getting groups
        			drupal_set_message(t('Error, could not fetch data'), 'error');
            			return false;
			}
			else
			{
				// extract all groups
        	    		$options = array();
				$groups = $jsonResult->data;
            			foreach($groups as $group)
            			{
                			$options[$group->id] = $group->name;
            			}

				// show the form to sync the groups
				$form = mailrelayForm($options, $apiKey);
				return $form;
			}
		}
	}
}

// validation for the sync screen, at least need to show one group
function mailrelayStartSync_validate($form, &$form_state)
{
	if (!is_array($form_state["values"]["groups"]) || count($form_state["values"]["groups"])==0)
	{
		// error
		form_set_error('', t('You must select at least one group to sync.'));
	}
}

// screen when we submit groups, and syncs users
function mailrelayStartSync_submit($form, &$form_state)
{
	// this script can not be aborted by user and has no time limit
	ignore_user_abort(true);
	set_time_limit(0);

	// have the api key and groups
	$apiKey = $form_state["values"]["apiKey"];
	$groups = $form_state["values"]["groups"];

	// read settings: hostname, username and password
	$hostname = variable_get('mailrelay_hostname', '');
        if (substr($hostname, 0, 7)=="http://") $hostname = substr($hostname, 7);

	$username = variable_get('mailrelay_username', '');
	$password = variable_get('mailrelay_password', '');

	$synced_users = 0;

        $url = 'http://'. $hostname .'/ccm/admin/api/version/2/&type=json';
        $curl = curl_init($url);

	// load drupal users
	$res_user = db_query("SELECT mail, name FROM {users}");
	foreach($res_user as $user)
	{
		if ($user->mail)
		{
	                // Call getSubscribers with the given mail, to check if the user is already in the system
        	        $params = array(
	        	        'function' => 'getSubscribers',
                        	'apiKey' => $apiKey,
                        	'email' => $user->mail
                	);

                        curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
                        curl_setopt($curl, CURLOPT_POST, 1);            
                        curl_setopt($curl, CURLOPT_POSTFIELDS, $params);

	                $headers = array(
        	                'X-Request-Origin: Drupal7|1.1|'.VERSION
        	        );
        	        curl_setopt($curl, CURLOPT_HTTPHEADER, $headers);

                        $result = curl_exec($curl);
                        $jsonResult = json_decode($result); 

                        if ((!$jsonResult->status) || (count($jsonResult->data)==0)) 
			{
                                $usuarios_mailrelay = new StdClass;
				$usuarios_mailrelay->email=null;
                        } 
			else 
			{
                                $data = $jsonResult->data;
                                $usuarios_mailrelay = $data[0];
                        }

                        //We check if the user already exists in the Mailrelay system
                        if($usuarios_mailrelay->email == $user->mail)
			{
                                // Call updateSubscriber
                                $params = array(
                                        'function' => 'updateSubscriber',
                                        'apiKey' => $apiKey,
                                        'id' => $usuarios_mailrelay->id,
                                        'email' => $user->mail,
                                        'name' => $user->name,
                                        'groups' => $groups

                                );
                                curl_setopt($curl, CURLOPT_POST, 1);
                                curl_setopt($curl, CURLOPT_POSTFIELDS, http_build_query($params, null, "&"));
                                curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
                                
		                $headers = array(
                		        'X-Request-Origin: Drupal7|1.1|'.VERSION
                		);
                		curl_setopt($curl, CURLOPT_HTTPHEADER, $headers);

                                $result = curl_exec($curl);
                                
                                $jsonResult = json_decode($result);
                        
				// if updated, add total
                                if ($jsonResult->status) 
				{
                                        $synced_users++;
                                }
                        }
			else
			{
                                // Call addSubscriber
                                $params = array(
                                        'function' => 'addSubscriber',
                                        'apiKey' => $apiKey,
                                        'email' => $user->mail,
                                        'name' => $user->name,
                                        'groups' => $groups

                                );
                                
                                curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
                                curl_setopt($curl, CURLOPT_POST, 1);
                                curl_setopt($curl, CURLOPT_POSTFIELDS, http_build_query($params, null, "&"));

        		        $headers = array(
	                	        'X-Request-Origin: Drupal7|1.1|'.VERSION
                		);
                		curl_setopt($curl, CURLOPT_HTTPHEADER, $headers);
                                $result = curl_exec($curl);
                                
                                $jsonResult = json_decode($result);     
                        
				// if added, add total
                                if ($jsonResult->status) 
				{
                                        $synced_users++;
                                }
                        }
			
		}
	}

	// show message
	$message = "<h1>".t("The sync process has finished: ")."</h1>".
		"<fieldset><legend>".t("Synced: ")."</legend>".
		"<ul class='adminformlist'><li><br /><b>";

	if ($synced_users == 1) 
	{
		$message.="1 ".t("user in total");
	}
	else 
	{
		$message.=$synced_users." ".t("users in total");
	}

	$message.="</b><br /><br /></li></ul></fieldset>";
	drupal_set_message(filter_xss($message), "status");
}
