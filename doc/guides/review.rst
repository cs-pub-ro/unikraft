===========================================
Reviewing Patches using the Mailing List
===========================================

.. _Patchwork: https://patchwork.unikraft.org/project/unikraft/list/

.. toctree::
    :maxdepth: 2


#########################
Review process
#########################

This is a section to better describe the entire review process using the `Mailing List <https://lists.xenproject.org/cgi-bin/mailman/listinfo/minios-devel>`_.
The purpose of a review is to make sure that everything is in order with the submitted 
patches and that the one who submitted the patches did not omit something that should be 
included for a better structure or performance of the project.

To be a reviewer, the first step would be to subscribe to the `Mailing List <https://lists.xenproject.org/cgi-bin/mailman/listinfo/minios-devel>`_.

After subscribing to the `Mailing List <https://lists.xenproject.org/cgi-bin/mailman/listinfo/minios-devel>`_ 
you will receive all the new patches submitted and you will be able to see the progress within the project.
You should only review the patches to which you are assigned. You can see this quite easily because 
for these patches you are assigned in CC.

You will see the patches in a series like this:

.. image:: /_static/image2.png

The mails that are numbered with 0/N represent the cover letter of the patches and mainly here
is a general description. It is not mandatory to reply to this email, only if you have
something to say in general about all the submitted patches.
After you finish the process review you should reply to all on each patch message on the
`Mailing List <https://lists.xenproject.org/cgi-bin/mailman/listinfo/minios-devel>`_. 
Place the review message at the end of each reply:

    Reviewed-by: **YOUR_NAME <YOUR_EMAIL>**

This is used by Patchwork_ to track the review process.

To take the patches locally and test them you have two options:

    * You can take the content of the mail and make a .diff file that you can then apply
      to the project you have configured locally. You can apply a diff file by command
      **git apply** /path/to/file.diff.
    * Another method is to use Patchwork_ which simplifies the process, but you need a
      git configuration which will be detailed in the **Managing Patches using Patchwork**
      section.


##################################
Responding to a non-received email
##################################

In case you want to review a patch sent on the `Mailing List <https://lists.xenproject.org/cgi-bin/mailman/listinfo/minios-devel>`_, 
but which you don’t have in your inbox, here is an example of how you can reply to that email.
Every email has a unique ID, which can be used to send a reply to that email. You can find the
Message-ID and the rest of the required information from Patchwork_, in the Headers section of
the patch that you want to reply to.

The first step is to create the email, as a text file, which should look like this:

.. image:: /_static/image5.png

After you created the email, let’s refer to it as reply.txt, and you set up git send-email
(see `this guideline <https://wiki.xenproject.org/wiki/Submitting_Xen_Project_Patches#Setting_up_git_send-email>`_)
use the following command to send it:

git send-email --to= **PATCH_SENDER** --cc=minios-devel@lists.xen.org reply.txt


#########################
Review guideline
#########################

In this section, you can see good practices and common mistakes after which 
you can look as a reviewer:

    * TBD
    * TBD
    * TBD