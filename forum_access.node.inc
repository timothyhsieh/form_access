<?php
// $Id$

/**
 * @file
 * forum_access.node.inc
 *
 * Include file for forum_access.module, containing (sub-)page handling
 * (form_alter) code for the node and comment forms as well as code
 * for temporarily assigning the 'forum moderator' role and managing
 * the resulting rights.
 */

/**
 * Rewrite the taxonomy item on the node form.
 */
function _forum_access_node_form(&$form, &$form_state) {
  global $user;
  $vid = _forum_access_get_vid();

  if (!isset($form['taxonomy_forums'])) {
    return;
  }

  // True node administrators are all powerful and do NOT get their forms rewritten here.
  if (user_access('bypass node access') && empty($user->_forum_access_moderator)) {
//TODO: revise
    return;
  }

  $roles = array_keys($user->roles);
  $tids = array();
  $result = db_query("SELECT tid FROM {forum_access} WHERE rid IN (:roles) AND grant_create = 1", array(
    ':roles' => $roles,
  ));
  foreach ($result as $obj) {
    $tids[$obj->tid] = $obj->tid;
  }

  // Also get all forums they happen to be able to moderate.
  $result = db_query("SELECT a.number AS tid FROM {acl} a INNER JOIN {acl_user} u ON a.acl_id = u.acl_id WHERE a.module = 'forum_access' AND u.uid = :uid", array(
    ':uid' => $user->uid,
  ));
  foreach ($result as $obj) {
    $tids[$obj->tid] = $obj->tid;
  }

  $nid = $form['nid']['#value'];
  if (!empty($nid)) {
    // Edit an existing node.
    if (!forum_access_access('update', $form['forum_tid']['#value']) && !user_access('edit any forum content') && !(user_access('edit own forum content') && $form['uid']['#value'] == $user->uid)) {
      drupal_access_denied();
      drupal_exit();
    }
    $forum_tid = $form['forum_tid']['#value'];
    $tids[$forum_tid] = $forum_tid;
  }
  else {
    // Create a new node -- ensure the forum they're trying to post to directly
    // is allowed, otherwise there will be much confusion.
    $forum_tid = arg(3);
    if (!empty($forum_tid) && is_numeric($forum_tid) && !isset($tids[$forum_tid])) {
      drupal_access_denied();
      drupal_exit();
    }
  }

  $form_options = &$form['taxonomy_forums'][$form['taxonomy_forums']['#language']]['#options'];
  $options = array();
  foreach ($form_options as $tid => $name) {
    if (!is_numeric($tid)) {
      $options[$tid] = $name;
    }
    elseif (is_object($name)) {
      foreach ($name->option as $sub_tid => $sub_name) {
        if (!empty($tids[$sub_tid])) {
          $options[$tid]->option[$sub_tid] = $sub_name;
        }
      }
    }
    elseif (isset($tids[$tid])) {
      $options[$tid] = $name;
    }
  }
  $form_options = $options;

  // Apply modifications for Moderators (by role or uid).
  if (!empty($user->_forum_access_moderator)) {
//TODO: revise comment and inner working
    // We gave this user the 'administer nodes' permission, which he doesn't
    // normally have. Remove controls that should be reserved to true node
    // administrators.
    _forum_access_disable_moderator(); // not needed anymore
    $allowed_elements = variable_get('forum_access_allowed_node_edit_elements', array('nid', 'vid', 'uid', 'created', 'type', 'changed', 'title', 'shadow', 'body_field', 'revision_information', 'form_build_id', 'form_token', 'form_id', 'comment_settings', 'taxonomy', 'attachments'));
    $allowed_options = variable_get('forum_access_allowed_node_edit_options', array('status', 'sticky', 'subscriptions_notify'));
    foreach (element_children($form) as $key) {
      switch ($key) {
        case 'options':
          foreach (element_children($form[$key]) as $key2) {
            if (array_search($key2, $allowed_options) === FALSE) {
              $form[$key][$key2]['#access'] = FALSE;
            }
          }
          break;
        case 'buttons':
          $tid = $form['taxonomy'][$vid]['#default_value'][0];
          if (!forum_access_access('update', $tid)) {
            $form['buttons']['submit']['#access'] = FALSE;
            $form['buttons']['preview']['#access'] = FALSE;
          }
          if (!forum_access_access('delete', $tid)) {
            $form['buttons']['delete']['#access'] = FALSE;
          }
          break;
        default:
          if (array_search($key, $allowed_elements) === FALSE) {
            $form[$key]['#access'] = FALSE;
          }
      }
    }
  }
}

/**
 * Check whether we're allowed to delete a node.
 */
//function _forum_access_form_node_delete_confirm_form($form, $form_state) {
//
//}

/**
 * Implements hook_node_view_alter().
 */
function _forum_access_node_view_alter(&$build, $tid) {
  //dpm($build, '_forum_access_node_view_alter() BEFORE:');
  if (!forum_access_access('create', $tid)) {
    unset($build['links']['#links']['comment-add']);
    $build['comments']['comment_form'] = NULL;
  }
  //dpm($build, '_forum_access_node_view_alter() AFTER:');
}

/**
 * Implements hook_comment_view_alter().
 */
function _forum_access_comment_view_alter(&$build, $tid) {
//  dpm($build, '_forum_access_comment_view_alter() BEFORE:');
//  if (!user_access('administer comments')) {
//    if (!forum_access_access('create', $tid)) {
//      unset($build['links']['comment']['#links']['comment-reply']);
//    }
//    if (!forum_access_access('update', $tid)) {
//      unset($build['links']['comment']['#links']['comment-edit']);
//    }
//    if (!forum_access_access('delete', $tid)) {
//      unset($build['links']['comment']['#links']['comment-delete']);
//    }
//  }
//  dpm($build, '_forum_access_comment_view_alter() AFTER:');
}

/**
 * Deny access if the user tries to enter a comment URL directly,
 * and sanitize the Administration options for users with Edit grants.
 */
function _forum_access_comment_form(&$form, &$form_state) {
  global $user;

  /*
  if (!user_access('bypass node access') && isset($form['nid']['#value'])) {
    $node = node_load($form['nid']['#value']);
    if ($tid = _forum_access_get_tid($node)) {
      if (!forum_access_access('create', $tid)) {
        switch (arg(0)) {
          case 'comment':
            drupal_access_denied();
            drupal_exit();
        }
      }
      else {/**/
//dpm($form, "FORM");
//dpm($form_state, "FORM_STATE");
        $comment = $form_state['comment'];
//dpm($comment, "COMMENT");
//dpm(format_date($comment->created, 'custom', 'Y-m-d H:i O'), "created:");
//dpm(format_date($comment->changed, 'custom', 'Y-m-d H:i O'), "changed:");
        if ($cid = $form['cid']['#value'] && isset($form['author']) && !empty($user->_forum_access_moderator)) {
          foreach (element_children($form['author']) as $key) {
            switch ($key) {
              case 'homepage':
              case 'status':
                break;
              case 'date':
                $form['author']['date']['#default_value'] = format_date($comment->created, 'custom', 'Y-m-d H:i O');
              default:
                //$form['author'][$key]['#enabled'] = FALSE;
                $form['author'][$key]['#disabled'] = TRUE;
                break;
            }
          }
        }
        /*
      }
    }
  }/**/
}

/*
 * Give the user the 'administer nodes' and 'administer comments' permissions for this request.
 */
//TODO: revise
function _forum_access_enable_moderator() {
  global $user;
  $perms = &drupal_static('user_access');
  if (!isset($perms[$user->uid])) {
    $perms[$user->uid]['administer comments'] = $perms[$user->uid]['administer nodes'] = 'forum_access';
  }
}

/*
 * Remove the moderator permissions.
 */
function _forum_access_disable_moderator() {
  global $user;
  $perms = &drupal_static('user_access');
  foreach (array('administer comments', 'administer nodes') as $perm) {
    if (isset($perms[$user->uid][$perm]) && $perms[$user->uid][$perm] == 'forum_access') {
      unset($perms[$user->uid][$perm]);
    }
  }
}

/**
 * Recreate comment links (they've already been themed), and
 * remove those that aren't accessible to the user.
 * /
function _forum_access_preprocess_comment(&$variables) {
  global $user;
  if (!empty($user->_forum_access_moderator)) {
    _forum_access_enable_moderator(); // this allows us to retrieve the comment links (without setting precedent!)
  }

  $tid = $variables['node']->tid;
  $links = module_invoke_all('link', 'comment', $variables['comment'], 0);

  if (!empty($user->_forum_access_moderator) && arg(0) == 'node' && arg(2) == NULL) {
    _forum_access_disable_moderator();
  }

  if (isset($links['comment_reply']) && (!preg_match('#<li class="[^"]*comment_reply[^"]*".*</li>#U', $variables['links']) || !forum_access_access('create', $tid, NULL, FALSE))) {
    unset($links['comment_reply']);
  }
  if (isset($links['comment_edit']) && !forum_access_access('update', $tid, NULL, FALSE) && !comment_access('edit', $variables['comment'])) {
    unset($links['comment_edit']);
  }
  if (isset($links['comment_delete']) && !forum_access_access('delete', $tid, NULL, FALSE) && !user_access('administer comments')) {
    unset($links['comment_delete']);
  }
  foreach (array_keys($links) as $link) {
    if (!in_array($link, array('comment_reply', 'comment_edit', 'comment_delete'))) {
      $link_preg_quote = preg_quote($link, '#');
      if (!preg_match('#<li class="[^"]*' . $link_preg_quote . '[^"]*"(.|\n)*</li>#U', $variables['links'])) {
        unset($links[$link]); // eliminate possible additional unknown links that came in for 'administer_comments'
      }
    }
  }
  if (empty($links)) {
    $links['comment_forbidden'] = array(
      'title' => theme('comment_post_forbidden', array('node' => $variables['node'])),
      'html'  => TRUE,
    );
  }
  $variables['links'] = theme('links', array('links' => $links));
}
/**/

